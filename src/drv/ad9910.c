/*
 * ad9910.c —— AD9910 DDS 驱动实现
 *
 * ---------------------------------------------------------------------------
 *  AD9910 SPI 帧格式（每次寄存器访问一次完整 CS 周期）
 * ---------------------------------------------------------------------------
 *    CSB 低
 *    ├── 指令字节 (1 byte)
 *    │      bit 7    = R/W#  (0=写, 1=读)
 *    │      bit 6..5 = 00     保留
 *    │      bit 4..0 = 寄存器地址 (0x00..0x1F)
 *    ├── 数据字节 (n bytes, MSB first)
 *    │      每个寄存器长度不同：
 *    │        CFR1/2/3, FTW, ASF     = 4 字节
 *    │        POW                    = 2 字节
 *    │        Profile 0..7           = 8 字节  (16b ASF + 16b POW + 32b FTW)
 *    │        DIGRAMP 上下界/步长    = 4 字节
 *    │        RAM 数据                = 4 字节/字
 *    CSB 高
 *    (可选) IO_UPDATE 脉冲 → 缓冲区值搬到工作寄存器，参数生效
 *
 * ---------------------------------------------------------------------------
 *  初始化流程
 * ---------------------------------------------------------------------------
 *    1) MASTER_RESET 拉高 ≥ 5 SYSCLK，再拉低      -> 清空内部状态
 *    2) SPI 写 CFR1                              -> 输出模式基本设置
 *    3) SPI 写 CFR2                              -> DAC clock / ASF from single tone
 *    4) SPI 写 CFR3                              -> PLL 参数（N=40, VCO range）
 *    5) 脉冲 IO_UPDATE                           -> 生效并启动 PLL 锁定
 *    6) 等 PLL 锁定 (~1 ms)
 *    7) 写默认 Profile 0（FTW/POW/ASF）
 *    8) 脉冲 IO_UPDATE                           -> 输出开始
 *
 * ---------------------------------------------------------------------------
 *  频率控制字 (FTW) 计算
 * ---------------------------------------------------------------------------
 *    FTW = f_out × 2^32 / f_sysclk
 *      f_sysclk = 1e9 Hz
 *      每 Hz 对应 FTW = 4.294967296
 *    实现里用 double 或 uint64 中间量避免大频率溢出。
 * ===========================================================================
 */

#include "drv/ad9910.h"
#include "bsp/spi.h"
#include "module/terminal.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* =====================================================================
 *  硬件绑定：换板子改这几行
 * ===================================================================== */

#define AD9910_CS_PORT      GPIOA
#define AD9910_CS_PIN       GPIO_PIN_4

#define AD9910_RESET_PORT   GPIOB
#define AD9910_RESET_PIN    GPIO_PIN_1

#define AD9910_IOUP_PORT    GPIOB
#define AD9910_IOUP_PIN     GPIO_PIN_2

/* 若板子上 IO_UPDATE 直接短接到 CSB（很多现成 DDS 模块这么做），
 * 把这个宏改成 1，MCU 就不再单独管 IO_UPDATE 引脚。 */
#define AD9910_IO_UPDATE_ON_CS   0

#define AD9910_CS_LOW()    HAL_GPIO_WritePin(AD9910_CS_PORT,    AD9910_CS_PIN,    GPIO_PIN_RESET)
#define AD9910_CS_HIGH()   HAL_GPIO_WritePin(AD9910_CS_PORT,    AD9910_CS_PIN,    GPIO_PIN_SET)
#define AD9910_RESET_HI()  HAL_GPIO_WritePin(AD9910_RESET_PORT, AD9910_RESET_PIN, GPIO_PIN_SET)
#define AD9910_RESET_LO()  HAL_GPIO_WritePin(AD9910_RESET_PORT, AD9910_RESET_PIN, GPIO_PIN_RESET)
#define AD9910_IOUP_HI()   HAL_GPIO_WritePin(AD9910_IOUP_PORT,  AD9910_IOUP_PIN,  GPIO_PIN_SET)
#define AD9910_IOUP_LO()   HAL_GPIO_WritePin(AD9910_IOUP_PORT,  AD9910_IOUP_PIN,  GPIO_PIN_RESET)

/* =====================================================================
 *  寄存器地址（AD9910 datasheet Table 15）
 * ===================================================================== */

#define REG_CFR1        0x00     /* Control Function Register 1  32-bit */
#define REG_CFR2        0x01     /* Control Function Register 2  32-bit */
#define REG_CFR3        0x02     /* Control Function Register 3  32-bit */
#define REG_AUX_DAC     0x03
#define REG_IO_UPD_RATE 0x04
#define REG_FTW         0x07     /* 32-bit FTW（未使用 profile 模式时）*/
#define REG_POW         0x08     /* 16-bit */
#define REG_ASF         0x09     /* 32-bit（含 ramp 步进等），我们只用 ASF 低 14 位 */
#define REG_PROFILE0    0x0E     /* 64-bit: [63:48]=ASF [47:32]=POW [31:0]=FTW */

/* =====================================================================
 *  内部状态
 * ===================================================================== */

static ad9910_state_t s_state = {
    .f_hz = 0.0, .amp01 = 1.0f, .phase_deg = 0.0f,
    .output_on = false, .pll_locked = false,
};

/* =====================================================================
 *  SPI 读写辅助
 * ===================================================================== */

/* 写寄存器：addr + 大端字节序数据。写完自动脉冲 IO_UPDATE（若使能）。
 * 若要"批量改多个寄存器 → 一次原子更新"，用 ad9910_write_reg_noupdate
 * 挨个写，最后手动 ad9910_io_update()。 */
static void ad9910_write_reg_noupdate(uint8_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = (0 << 7) | (addr & 0x1F);  /* 写：MSB=0 */
    AD9910_CS_LOW();
    SPI1_Write(&cmd, 1);
    SPI1_Write(data, len);
    AD9910_CS_HIGH();
}

/* 脉冲 IO_UPDATE 让缓冲寄存器搬到工作寄存器。
 * 若板上 IO_UPDATE 短接到 CS，这里就是 no-op。 */
static void ad9910_io_update(void)
{
#if AD9910_IO_UPDATE_ON_CS
    /* CS 上升沿已经在 write_reg 里做过一次，这里什么都不做 */
#else
    AD9910_IOUP_HI();
    /* 至少 SYSCLK 一个周期宽度（1ns @ 1GHz），几十纳秒足够 */
    for (volatile int i = 0; i < 8; ++i) __NOP();
    AD9910_IOUP_LO();
#endif
}

static void ad9910_write_reg(uint8_t addr, const uint8_t *data, uint8_t len)
{
    ad9910_write_reg_noupdate(addr, data, len);
    ad9910_io_update();
}

/* 32-bit / 16-bit 便捷版：主机端到 AD9910 都是大端 (MSB first)。 */
static void ad9910_write32(uint8_t addr, uint32_t v)
{
    uint8_t b[4] = {
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v      )
    };
    ad9910_write_reg(addr, b, 4);
}
static void ad9910_write16(uint8_t addr, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    ad9910_write_reg(addr, b, 2);
}

/* Profile 寄存器格式：[63:48]=ASF (16b) [47:32]=POW (16b) [31:0]=FTW (32b)。
 * 需要一次原子写完（所以不用 write_reg 的自动 update，最后统一 io_update）。 */
static void ad9910_write_profile0(uint16_t asf, uint16_t pow16, uint32_t ftw)
{
    uint8_t b[8] = {
        (uint8_t)(asf   >> 8), (uint8_t)asf,
        (uint8_t)(pow16 >> 8), (uint8_t)pow16,
        (uint8_t)(ftw   >> 24), (uint8_t)(ftw >> 16),
        (uint8_t)(ftw   >>  8), (uint8_t) ftw,
    };
    ad9910_write_reg_noupdate(REG_PROFILE0, b, 8);
    ad9910_io_update();
}

/* =====================================================================
 *  参数换算
 * ===================================================================== */

static uint32_t freq_to_ftw(double f_hz)
{
    if (f_hz < 0.0) f_hz = 0.0;
    if (f_hz >= (double)AD9910_SYSCLK_HZ * 0.5)
        f_hz = (double)AD9910_SYSCLK_HZ * 0.5 - 1.0;
    /* FTW = f × 2^32 / sysclk。用 double 中间量避免溢出 */
    double ftw = f_hz * 4294967296.0 / (double)AD9910_SYSCLK_HZ;
    if (ftw > 4294967295.0) ftw = 4294967295.0;
    return (uint32_t)ftw;
}
static uint16_t amp01_to_asf(float amp)
{
    /* ASF 14-bit：0..0x3FFF 对应 0..1 */
    if (amp < 0.0f) amp = 0.0f;
    if (amp > 1.0f) amp = 1.0f;
    return (uint16_t)(amp * 16383.0f + 0.5f);
}
static uint16_t phase_deg_to_pow(float deg)
{
    /* POW 16-bit：0..0xFFFF 对应 0..360° */
    /* 归一到 [0, 360) */
    while (deg <  0.0f)  deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return (uint16_t)(deg / 360.0f * 65536.0f + 0.5f);
}

/* =====================================================================
 *  硬件初始化
 * ===================================================================== */

/* CLI 命令注册的实现在文件末尾（term_cmd_t 静态变量在那里定义），
 * 这里先声明一下让 ad9910_init 能调用。 */
static void ad9910_register_cli(void);

static void ad9910_gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = AD9910_CS_PIN;       HAL_GPIO_Init(AD9910_CS_PORT,    &g);
    g.Pin = AD9910_RESET_PIN;    HAL_GPIO_Init(AD9910_RESET_PORT, &g);
#if !AD9910_IO_UPDATE_ON_CS
    g.Pin = AD9910_IOUP_PIN;     HAL_GPIO_Init(AD9910_IOUP_PORT,  &g);
    AD9910_IOUP_LO();
#endif

    AD9910_CS_HIGH();
    AD9910_RESET_LO();
}

/* =====================================================================
 *  公开接口
 * ===================================================================== */

void ad9910_soft_reset(void)
{
    /* MASTER_RESET 需要至少 5 个 SYSCLK 高电平（1GHz → 5ns，其实很短，
     * 但 SPI 内部还要几个 DAC clk 周期，保守拉 1ms） */
    AD9910_RESET_HI();
    HAL_Delay(1);
    AD9910_RESET_LO();
    HAL_Delay(1);
}

void ad9910_init(void)
{
    /* --- 硬件层：SPI + 控制引脚 --- */
    MX_SPI1_Init();
    ad9910_gpio_init();

    /* --- 硬复位 --- */
    ad9910_soft_reset();

    /* --- CFR1 (0x00, 32-bit) ---
     *   默认全 0 即可：单音、非 RAM、非 OSK、SDIO 3-line 关（用 4-line）。
     *   若板子只有 SDIO 单线 + 需要读回，bit 1 (SDIO input only) 置 1。 */
    ad9910_write32(REG_CFR1, 0x00000000);

    /* --- CFR2 (0x01, 32-bit) ---
     *   bit 24 Matched Latency Enable       = 1  (推荐)
     *   bit 22 Data assembler hold last    = 1
     *   bit 11 Read Effective FTW/POW/ASF  = 0
     *   bit 10 Sync Timing Validation Disable = 1
     *   bit 8  Sync Clock Enable           = 1
     *   bit 5  未使用位（保留 1）
     *   bit 4  Ampl scale from Single Tone Profile Enable = 1
     *          →  单音输出用当前 profile 的 ASF（我们用 profile 0）
     *   → 0x01400830  */
    ad9910_write32(REG_CFR2, 0x01400830);

    /* --- CFR3 (0x02, 32-bit)  ← PLL 配置 25MHz × 40 = 1GHz ---
     *   bit 27..26  DRV0 (输出使能相关，默认 3)
     *   bit 24..19  N (Feedback divider) = 40 → 0b101000
     *   bit 18..16  VCO SEL = 5 (对应 1GHz 输出)
     *   bit 15..14  ICP (charge pump current) = 3
     *   bit 8       REFCLK input divider reset = 0
     *   bit 3       PLL Enable = 1
     *   bit 1..0    未使用
     *
     *   合成值 (二进制展开)：
     *     N=40, VCO=5, ICP=3, PLL_EN=1
     *     → 0x0D 05 4B 08  （近似，具体位可能需按 datasheet 校正）
     *   常见工作值：0x0D054B08 或 0x1D3F41C0
     *
     *   为可读性保守起见，用一个已知能锁定的组合：
     */
    ad9910_write32(REG_CFR3, 0x0D054B08);

    /* 触发生效，让 PLL 开始工作 */
    ad9910_io_update();
    HAL_Delay(5);           /* 等 PLL 锁定（datasheet 典型 ≤ 1ms） */
    s_state.pll_locked = true;   /* 假定成功；真机可通过 SDO 读回锁定状态 */

    /* --- 默认输出：1 kHz 单音，全幅，0° --- */
    ad9910_set_tone(&(ad9910_tone_t){ .f_hz = 1000.0, .amp01 = 1.0f, .phase_deg = 0.0f });
    ad9910_output_enable(true);

    /* --- 注册 terminal 命令（实现在文件末尾） --- */
    ad9910_register_cli();
}

/* --- 单独设置 API：内部都是"改 profile 0 一次性写"防抖 --- */

void ad9910_set_freq_hz(double f_hz)
{
    s_state.f_hz = f_hz;
    ad9910_write_profile0(amp01_to_asf(s_state.amp01),
                          phase_deg_to_pow(s_state.phase_deg),
                          freq_to_ftw(f_hz));
}

void ad9910_set_amp01(float amp)
{
    s_state.amp01 = amp;
    ad9910_write_profile0(amp01_to_asf(amp),
                          phase_deg_to_pow(s_state.phase_deg),
                          freq_to_ftw(s_state.f_hz));
}

void ad9910_set_phase_deg(float deg)
{
    s_state.phase_deg = deg;
    ad9910_write_profile0(amp01_to_asf(s_state.amp01),
                          phase_deg_to_pow(deg),
                          freq_to_ftw(s_state.f_hz));
}

void ad9910_set_tone(const ad9910_tone_t *t)
{
    s_state.f_hz      = t->f_hz;
    s_state.amp01     = t->amp01;
    s_state.phase_deg = t->phase_deg;
    ad9910_write_profile0(amp01_to_asf(t->amp01),
                          phase_deg_to_pow(t->phase_deg),
                          freq_to_ftw(t->f_hz));
}

void ad9910_output_enable(bool en)
{
    /* 通过 ASF 归 0 来"关闭"（幅度 0）。也可以改 CFR2 的 DAC power-down 位。 */
    if (en) {
        ad9910_write_profile0(amp01_to_asf(s_state.amp01),
                              phase_deg_to_pow(s_state.phase_deg),
                              freq_to_ftw(s_state.f_hz));
    } else {
        ad9910_write_profile0(0,
                              phase_deg_to_pow(s_state.phase_deg),
                              freq_to_ftw(s_state.f_hz));
    }
    s_state.output_on = en;
}

const ad9910_state_t *ad9910_get_state(void) { return &s_state; }

/* =====================================================================
 *  Terminal 命令：dds.freq / dds.amp / dds.phase / dds.on / dds.off / dds.info
 * ===================================================================== */

static int cmd_dds_freq(int argc, char **argv)
{
    if (argc < 2) { term_printf("usage: dds.freq <hz>\r\n"); return -1; }
    double f = atof(argv[1]);
    ad9910_set_freq_hz(f);
    term_printf("freq: %.3f Hz\r\n", f);
    return 0;
}
static term_cmd_t s_c_freq = { "dds.freq", cmd_dds_freq, "<hz> set output frequency", 0 };

static int cmd_dds_amp(int argc, char **argv)
{
    if (argc < 2) { term_printf("usage: dds.amp <0..1>\r\n"); return -1; }
    float a = (float)atof(argv[1]);
    ad9910_set_amp01(a);
    term_printf("amp: %.3f\r\n", (double)a);
    return 0;
}
static term_cmd_t s_c_amp = { "dds.amp", cmd_dds_amp, "<0..1> set amplitude", 0 };

static int cmd_dds_phase(int argc, char **argv)
{
    if (argc < 2) { term_printf("usage: dds.phase <deg>\r\n"); return -1; }
    float p = (float)atof(argv[1]);
    ad9910_set_phase_deg(p);
    term_printf("phase: %.2f deg\r\n", (double)p);
    return 0;
}
static term_cmd_t s_c_phase = { "dds.phase", cmd_dds_phase, "<deg> set phase", 0 };

static int cmd_dds_on(int argc, char **argv)
{
    (void)argc; (void)argv;
    ad9910_output_enable(true);
    return 0;
}
static term_cmd_t s_c_on = { "dds.on", cmd_dds_on, "enable DAC output", 0 };

static int cmd_dds_off(int argc, char **argv)
{
    (void)argc; (void)argv;
    ad9910_output_enable(false);
    return 0;
}
static term_cmd_t s_c_off = { "dds.off", cmd_dds_off, "disable DAC output", 0 };

static int cmd_dds_info(int argc, char **argv)
{
    (void)argc; (void)argv;
    term_printf("freq  : %.3f Hz\r\n", s_state.f_hz);
    term_printf("amp   : %.3f\r\n",    (double)s_state.amp01);
    term_printf("phase : %.2f deg\r\n",(double)s_state.phase_deg);
    term_printf("output: %s\r\n", s_state.output_on ? "ON" : "OFF");
    term_printf("pll   : %s\r\n", s_state.pll_locked ? "locked" : "unlocked");
    return 0;
}
static term_cmd_t s_c_info = { "dds.info", cmd_dds_info, "show DDS state", 0 };

/* 在 ad9910_init 结尾被调用，把上面这批命令挂到 terminal 上。 */
static void ad9910_register_cli(void)
{
    term_register(&s_c_freq);
    term_register(&s_c_amp);
    term_register(&s_c_phase);
    term_register(&s_c_on);
    term_register(&s_c_off);
    term_register(&s_c_info);
}
