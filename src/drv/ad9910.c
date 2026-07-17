/*
 * ad9910.c —— AD9910 DDS 驱动实现 (硬件层)
 *
 * 只做寄存器级动作。业务模式 (单音/RAM任意波/DRG扫频/跳频/OSK) 由 module/dds
 * 层拼装本文件的 API 得到。
 *
 * ---------------------------------------------------------------------------
 *  与 example (resources/AD9910Demo v1.1) 的对齐点
 * ---------------------------------------------------------------------------
 *    - PLL 参考 40 MHz，倍频 25 → SYSCLK = 1 GHz          → CFR3 = 0x050F4132
 *    - CFR1 默认 0x00000000  = 单音、profile 0 起作用
 *    - CFR2 默认 0x01400820  = 匹配延迟 + SYNC_CLK + ASF from profile
 *    - Profile 0 大端 8 字节 [ASF:16][POW:16][FTW:32]
 *    - FTW = f × 2^32 / 1e9 ≈ f × 4.294967296  (与 example Freq_convert 一致)
 *
 *  注：example 用软件 SPI (PA0..PA7 全部 GPIO 位翻)。本工程用硬件 SPI1 (PA5/6/7)
 *  + PA4 做 CS，速度快、抖动小，也更省 CPU。
 * ---------------------------------------------------------------------------
 */

#include "drv/ad9910.h"
#include "bsp/spi.h"
#include "bsp/uart.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <math.h>

/* ==================================================================
 *  硬件绑定 —— 换板子改这里
 * ================================================================== */
#define AD9910_CS_PORT        GPIOA
#define AD9910_CS_PIN         GPIO_PIN_4

#define AD9910_RESET_PORT     GPIOC
#define AD9910_RESET_PIN      GPIO_PIN_3

#define AD9910_IOUP_PORT      GPIOC
#define AD9910_IOUP_PIN       GPIO_PIN_4

#define AD9910_PROF_PORT      GPIOB
#define AD9910_PROF0_PIN      GPIO_PIN_12
#define AD9910_PROF1_PIN      GPIO_PIN_13
#define AD9910_PROF2_PIN      GPIO_PIN_14

#define AD9910_OSK_PORT       GPIOB
#define AD9910_OSK_PIN        GPIO_PIN_15

/* --- 下面三根 "AD9910 datasheet 明确要求不能悬空" 的引脚 ---
 *  就算单音模式不用它们的功能，也必须由 MCU 拉低 (或硬件短到 GND)，
 *  否则芯片可能随机进入 power-down / 冻结 DRG / 反向斜坡。*/
#define AD9910_PWR_PORT       GPIOC
#define AD9910_PWR_PIN        GPIO_PIN_0    /* EXT_PWR_DWN, 高 → 芯片关机 */

#define AD9910_DPH_PORT       GPIOC
#define AD9910_DPH_PIN        GPIO_PIN_1    /* DRHOLD,      高 → DRG 冻结 */

#define AD9910_DRC_PORT       GPIOC
#define AD9910_DRC_PIN        GPIO_PIN_2    /* DRCTL,       控制 DRG 方向 */

/* GPIO 快速宏 (可读性 + 便于日后换用 BSRR 汇编内联) */
#define AD9910_CS_LOW()    HAL_GPIO_WritePin(AD9910_CS_PORT,    AD9910_CS_PIN,    GPIO_PIN_RESET)
#define AD9910_CS_HIGH()   HAL_GPIO_WritePin(AD9910_CS_PORT,    AD9910_CS_PIN,    GPIO_PIN_SET)
#define AD9910_RESET_HI()  HAL_GPIO_WritePin(AD9910_RESET_PORT, AD9910_RESET_PIN, GPIO_PIN_SET)
#define AD9910_RESET_LO()  HAL_GPIO_WritePin(AD9910_RESET_PORT, AD9910_RESET_PIN, GPIO_PIN_RESET)
#define AD9910_IOUP_HI()   HAL_GPIO_WritePin(AD9910_IOUP_PORT,  AD9910_IOUP_PIN,  GPIO_PIN_SET)
#define AD9910_IOUP_LO()   HAL_GPIO_WritePin(AD9910_IOUP_PORT,  AD9910_IOUP_PIN,  GPIO_PIN_RESET)
#define AD9910_OSK_HI()    HAL_GPIO_WritePin(AD9910_OSK_PORT,   AD9910_OSK_PIN,   GPIO_PIN_SET)
#define AD9910_OSK_LO()    HAL_GPIO_WritePin(AD9910_OSK_PORT,   AD9910_OSK_PIN,   GPIO_PIN_RESET)

/* 极短纳秒级延时 —— 用于满足 tCS/tIO_UPDATE 时序 */
static inline void ad9910_short_delay(void) {
    for (volatile int i = 0; i < 8; ++i) __NOP();
}

/* ==================================================================
 *  内部状态
 * ================================================================== */
static ad9910_state_t s_state = {
    .f_hz = 0.0, .amp01 = 0.0f, .phase_deg = 0.0f,
    .last_ftw = 0, .last_asf14 = 0, .last_pow = 0,
    .pll_locked = false, .output_on = false, .profile_sel = 0,
    .cfr3_readback = 0,
};

/* ==================================================================
 *  I/O UPDATE
 * ================================================================== */
void ad9910_io_update(void)
{
#if AD9910_IO_UPDATE_ON_CS
    /* IO_UPDATE 硬短接 CSB，CS 上升沿即触发。上层 write_reg 已把 CS 拉高，
     * 这里无事可做。*/
#else
    AD9910_IOUP_HI();
    ad9910_short_delay();
    AD9910_IOUP_LO();
#endif
}

/* ==================================================================
 *  SPI 寄存器读写基元
 *
 *  帧格式：CSB↓ [Instr:1B] [Data:nB, MSB first] CSB↑
 *    Instr = (R/W#<<7) | (Addr & 0x1F)
 * ================================================================== */
void ad9910_write_reg_noupdate(uint8_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = (uint8_t)((0U << 7) | (addr & 0x1F));    /* 写 */
    AD9910_CS_LOW();
    SPI1_Write(&cmd, 1);
    SPI1_Write(data, len);
    AD9910_CS_HIGH();
}

void ad9910_write_reg(uint8_t addr, const uint8_t *data, uint8_t len)
{
    ad9910_write_reg_noupdate(addr, data, len);
    ad9910_io_update();
}

void ad9910_write32(uint8_t addr, uint32_t v)
{
    uint8_t b[4] = {
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v      ),
    };
    ad9910_write_reg(addr, b, 4);
}

void ad9910_write16(uint8_t addr, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    ad9910_write_reg(addr, b, 2);
}

void ad9910_write64(uint8_t addr, uint64_t v)
{
    uint8_t b[8] = {
        (uint8_t)(v >> 56), (uint8_t)(v >> 48),
        (uint8_t)(v >> 40), (uint8_t)(v >> 32),
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v      ),
    };
    ad9910_write_reg(addr, b, 8);
}

/* ==================================================================
 *  SPI 读回 (4 线模式)
 *
 *  读时序:
 *    CSB↓ | Instr(1B, bit7=1 表示读) | 主机时钟继续但 MOSI 发 dummy 0 |
 *      AD9910 从 SDO 输出 N 字节数据 (MSB first) | CSB↑
 *
 *  用 SPI1_Xchg 做 (1 + N) 字节全双工交换，指令字节的 RX 无意义丢弃，
 *  之后的 N 字节即返回数据。
 *
 *  前提：CFR1 bit 1 (SDIO Input Only) 必须置 1，让 AD9910 从 SDO (而非 SDIO)
 *  输出读数据。ad9910_init 里第一次写 CFR1 就打开这个位。
 * ================================================================== */
void ad9910_read_reg(uint8_t addr, uint8_t *buf, uint8_t len)
{
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    tx[0] = (uint8_t)((1U << 7) | (addr & 0x1F));   /* R/W# = 1 → 读 */
    /* tx[1..len] 默认 0，作为 dummy 送出去，提供时钟给 AD9910 */

    AD9910_CS_LOW();
    SPI1_Xchg(tx, rx, (uint16_t)(1 + len));
    AD9910_CS_HIGH();

    /* rx[0] 是指令字节期间收到的，忽略 */
    for (uint8_t i = 0; i < len; ++i) buf[i] = rx[i + 1];
}

uint32_t ad9910_read32(uint8_t addr)
{
    uint8_t b[4] = {0};
    ad9910_read_reg(addr, b, 4);
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) | ((uint32_t)b[3]);
}

bool ad9910_pll_is_locked(void) { return s_state.pll_locked; }

/* ==================================================================
 *  参数换算 —— 全部允许"公开给业务层"复用
 * ================================================================== */
uint32_t ad9910_freq_to_ftw(double f_hz)
{
    if (f_hz < 0.0) f_hz = 0.0;
    /* 硬饱和到 Nyquist；datasheet 建议 f ≤ 0.4·SYSCLK */
    double f_max = (double)AD9910_SYSCLK_HZ * 0.5 - 1.0;
    if (f_hz > f_max) f_hz = f_max;
    double ftw = f_hz * 4294967296.0 / (double)AD9910_SYSCLK_HZ;
    if (ftw > 4294967295.0) ftw = 4294967295.0;
    return (uint32_t)ftw;
}

uint16_t ad9910_amp01_to_asf(float amp01)
{
    if (amp01 < 0.0f) amp01 = 0.0f;
    if (amp01 > 1.0f) amp01 = 1.0f;
    return (uint16_t)(amp01 * 16383.0f + 0.5f);   /* 14-bit */
}

uint16_t ad9910_phase_deg_to_pow(float deg)
{
    /* 折到 [0, 360) */
    while (deg <    0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return (uint16_t)(deg / 360.0f * 65536.0f + 0.5f);
}

/* ==================================================================
 *  GPIO 初始化 (CS/RESET/IOUP/PROFILE/OSK/PWR/DPH/DRC)
 * ================================================================== */
static void ad9910_gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    /* CS */
    g.Pin = AD9910_CS_PIN;       HAL_GPIO_Init(AD9910_CS_PORT,    &g);
    AD9910_CS_HIGH();

    /* RESET */
    g.Pin = AD9910_RESET_PIN;    HAL_GPIO_Init(AD9910_RESET_PORT, &g);
    AD9910_RESET_LO();

    /* IO_UPDATE */
#if !AD9910_IO_UPDATE_ON_CS
    g.Pin = AD9910_IOUP_PIN;     HAL_GPIO_Init(AD9910_IOUP_PORT,  &g);
    AD9910_IOUP_LO();
#endif

    /* PROFILE[2:0] & OSK 默认低电平 = profile 0 & 无键控 */
    g.Pin = AD9910_PROF0_PIN | AD9910_PROF1_PIN | AD9910_PROF2_PIN;
    HAL_GPIO_Init(AD9910_PROF_PORT, &g);
    HAL_GPIO_WritePin(AD9910_PROF_PORT,
                      AD9910_PROF0_PIN | AD9910_PROF1_PIN | AD9910_PROF2_PIN,
                      GPIO_PIN_RESET);

    g.Pin = AD9910_OSK_PIN;      HAL_GPIO_Init(AD9910_OSK_PORT,   &g);
    AD9910_OSK_LO();

    /* PWR / DPH / DRC —— datasheet 要求"不用则接地"，用 MCU 拉低效果一样 */
    g.Pin = AD9910_PWR_PIN;      HAL_GPIO_Init(AD9910_PWR_PORT,   &g);
    HAL_GPIO_WritePin(AD9910_PWR_PORT, AD9910_PWR_PIN, GPIO_PIN_RESET);

    g.Pin = AD9910_DPH_PIN;      HAL_GPIO_Init(AD9910_DPH_PORT,   &g);
    HAL_GPIO_WritePin(AD9910_DPH_PORT, AD9910_DPH_PIN, GPIO_PIN_RESET);

    g.Pin = AD9910_DRC_PIN;      HAL_GPIO_Init(AD9910_DRC_PORT,   &g);
    HAL_GPIO_WritePin(AD9910_DRC_PORT, AD9910_DRC_PIN, GPIO_PIN_RESET);
}

/* ==================================================================
 *  生命周期
 * ================================================================== */
void ad9910_soft_reset(void)
{
    /* MASTER_RESET 需要至少 5 SYSCLK 高电平；SYSCLK=1GHz 时 5ns，
     * 保守拉 1 ms 兼顾 SPI/DAC 内部复位。 */
    AD9910_RESET_HI();
    HAL_Delay(1);
    AD9910_RESET_LO();
    HAL_Delay(1);
}

void ad9910_init(void)
{
    /* ---- 底层外设 ---- */
    MX_SPI1_Init();
    ad9910_gpio_init();

    /* ---- 硬复位 ---- */
    ad9910_soft_reset();

    /* ---- CFR1: 单音模式 (RAM 关闭, OSK 关闭, SDIO 保持双向) ---- */
    ad9910_write32(AD9910_REG_CFR1, 0x00000000UL);

    /* ---- CFR2: 匹配延迟 + SYNC_CLK + "ASF from single-tone profile" 使能 ---- */
    ad9910_write32(AD9910_REG_CFR2, AD9910_CFR2_BASE);

    /* ---- CFR3: PLL 40 MHz × 25 = 1 GHz ---- */
    ad9910_write32(AD9910_REG_CFR3, AD9910_CFR3_VALUE);

    /* PLL 锁定 datasheet 典型 < 1 ms，保守等 5 ms。
     * 本模块 SDO 没引出，无法 SPI 读回校验，只能假定锁定成功。
     * 判断 PLL 是否真锁只能靠"示波器上有没有 1kHz 正弦"。 */
    ad9910_io_update();
    HAL_Delay(5);
    s_state.pll_locked    = true;   /* 假定 */
    s_state.cfr3_readback = 0;      /* 未读 */

    /* ---- 默认输出：静音 (ASF=0)，profile 0 频率/相位记为 0 ---- */
    ad9910_profile_write(0, 0, 0, 0);
    ad9910_profile_select(0);
    s_state.output_on = false;
}

/* ==================================================================
 *  Profile 写入 & 引脚选择
 * ================================================================== */
void ad9910_profile_write(uint8_t n, uint16_t asf14, uint16_t pow16, uint32_t ftw)
{
    /* Profile 单音格式: [63:48]=ASF, [47:32]=POW, [31:0]=FTW，均大端。
     * ASF 只低 14 bit 有效，高 2 bit 是 Amp Ramp 相关位；此处置 0 = 不做 ramp。*/
    uint64_t v = ((uint64_t)(asf14 & 0x3FFF) << 48)
               | ((uint64_t)pow16           << 32)
               | ((uint64_t)ftw);
    ad9910_write64(AD9910_REG_PROFILE(n & 0x7), v);

    if ((n & 0x7) == s_state.profile_sel) {
        s_state.last_asf14 = asf14 & 0x3FFF;
        s_state.last_pow   = pow16;
        s_state.last_ftw   = ftw;
    }
}

void ad9910_profile_select(uint8_t n)
{
    n &= 0x7;
    HAL_GPIO_WritePin(AD9910_PROF_PORT, AD9910_PROF0_PIN, (n & 0x1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9910_PROF_PORT, AD9910_PROF1_PIN, (n & 0x2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9910_PROF_PORT, AD9910_PROF2_PIN, (n & 0x4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    s_state.profile_sel = n;
}

/* --- 单音三要素快捷接口 --- */
static void write_current_profile0(void)
{
    ad9910_profile_write(0,
        ad9910_amp01_to_asf   (s_state.amp01),
        ad9910_phase_deg_to_pow(s_state.phase_deg),
        ad9910_freq_to_ftw    (s_state.f_hz));
}

void ad9910_set_freq_hz  (double f)   { s_state.f_hz      = f; write_current_profile0(); }
void ad9910_set_amp01    (float  a)   { s_state.amp01     = a; write_current_profile0(); }
void ad9910_set_phase_deg(float  d)   { s_state.phase_deg = d; write_current_profile0(); }

void ad9910_set_tone(const ad9910_tone_t *t)
{
    s_state.f_hz      = t->f_hz;
    s_state.amp01     = t->amp01;
    s_state.phase_deg = t->phase_deg;
    write_current_profile0();
    s_state.output_on = (t->amp01 > 0.0f);
}

void ad9910_output_enable(bool en)
{
    /* 通过 ASF=0 (静音) 实现快关，避免关 DAC power-down 造成的启停 glitch。*/
    ad9910_profile_write(0,
        en ? ad9910_amp01_to_asf(s_state.amp01) : 0,
        ad9910_phase_deg_to_pow (s_state.phase_deg),
        ad9910_freq_to_ftw      (s_state.f_hz));
    s_state.output_on = en;
}

/* ==================================================================
 *  RAM 模式
 * ==================================================================
 *
 *  流程 (datasheet Fig 39):
 *    1) 关 RAM (若之前开着)  → 便于修改 profile 参数
 *    2) 写 profile n = RAM playback 参数 (start/end/rate/mode)
 *    3) 选中该 profile (PROFILE 引脚)
 *    4) SPI 连写 REG_RAM，AD9910 内部按序写入 RAM[start..end]
 *    5) 写 CFR1 使能 RAM_ENABLE + 目标位
 *    6) IO_UPDATE → 开始 playback
 */

void ad9910_ram_write(uint16_t start_addr, const uint32_t *data, uint16_t n)
{
    /* 先在 profile 0 里写 start_addr —— AD9910 是通过 profile.start_addr 决定
     * 后续 REG_RAM 写入从哪里开始的。这里做一个最简 profile：只填 start_addr。
     * 真正的 playback 参数在 ad9910_ram_profile_config 里再写一遍。*/
    ad9910_ram_profile_t tmp = {
        .start_addr = start_addr,
        .end_addr   = (uint16_t)(start_addr + n - 1),
        .rate_divider = 0,
        .mode = AD9910_RAM_MODE_DIRECT_SWITCH,
        .no_dwell_high = false, .zero_crossing = false,
    };
    ad9910_ram_profile_config(0, &tmp);

    /* 连续 SPI 写 REG_RAM = 0x16，数据 MSB first，每字 4 字节。CSB 保持低期间
     * 内部地址自增。*/
    uint8_t cmd = (uint8_t)((0U << 7) | (AD9910_REG_RAM & 0x1F));
    AD9910_CS_LOW();
    SPI1_Write(&cmd, 1);
    for (uint16_t i = 0; i < n; ++i) {
        uint32_t v = data[i];
        uint8_t b[4] = {
            (uint8_t)(v >> 24), (uint8_t)(v >> 16),
            (uint8_t)(v >>  8), (uint8_t)(v      ),
        };
        SPI1_Write(b, 4);
    }
    AD9910_CS_HIGH();
    ad9910_io_update();
}

void ad9910_ram_profile_config(uint8_t profile_n, const ad9910_ram_profile_t *cfg)
{
    /* RAM playback profile 位分配 (与实测例子对齐: 三角波例子 profile =
     *   0x00 00 01 FF C0 00 00 04, 反解得到下面的位分配):
     *
     *   [63:56] Reserved
     *   [55:40] Address Ramp Rate M         (16 bit) —— 播放速率
     *   [39:30] Waveform End Address        (10 bit, 顶部对齐在 [39:24] 的 16-bit 字段)
     *   [29:24] Reserved (低位补零)
     *   [23:14] Waveform Start Address      (10 bit, 顶部对齐在 [23:8])
     *   [13:8]  Reserved (低位补零)
     *   [7:5]   Reserved
     *   [4]     No-dwell High
     *   [3]     Zero-crossing Enable
     *   [2:0]   RAM Playback Mode
     */
    uint64_t v = 0;
    v |= ((uint64_t)(cfg->rate_divider & 0xFFFF) << 40);  /* rate M at [55:40] */
    v |= ((uint64_t)(cfg->end_addr    & 0x3FF)  << 30);   /* end   at [39:30] */
    v |= ((uint64_t)(cfg->start_addr  & 0x3FF)  << 14);   /* start at [23:14] */
    if (cfg->no_dwell_high) v |= (1ULL << 4);
    if (cfg->zero_crossing) v |= (1ULL << 3);
    v |= ((uint64_t)(cfg->mode & 0x7));

    ad9910_write64(AD9910_REG_PROFILE(profile_n & 0x7), v);
}

void ad9910_ram_enable(uint32_t dest_mask)
{
    /* 与实测例子完全对齐:
     *   CFR1 = 0xC0401000 = bit31(RAM_EN) | bit30(dest ASF) | bit22(禁 sinc) | bit12(清相位)
     *   CFR2 里 "ASF from single-tone profile" bit 5 关掉
     *   FTW / POW 寄存器显式清 0
     *
     *   bit 12 (Clear Phase Accumulator) 强制相位累加器归零, 让 cos(phase) = 1,
     *   这样 DAC 摆幅达到 IOUT_FS 极限 (跟单音模式峰峰值一致). */
    ad9910_write32(AD9910_REG_CFR2, AD9910_CFR2_BASE & ~(1UL << 5));
    ad9910_write32(AD9910_REG_FTW,  0x00000000UL);
    ad9910_write16(AD9910_REG_POW,  0x0000);       /* 相位偏移强制 = 0 */

    /* CFR1 = RAM_ENABLE | dest | bit22 | bit12 (清相位累加器) */
    uint32_t cfr1 = AD9910_CFR1_RAM_ENABLE
                  | (dest_mask & (3UL << 29))
                  | (1UL << 22)
                  | (1UL << 12);
    ad9910_write32(AD9910_REG_CFR1, cfr1);
}

void ad9910_ram_disable(void)
{
    ad9910_write32(AD9910_REG_CFR1, 0x00000000UL);
    /* 恢复单音模式的 CFR2 (bit 5 = 1, ASF 又回到 profile 提供) */
    ad9910_write32(AD9910_REG_CFR2, AD9910_CFR2_BASE);
}

/* ==================================================================
 *  DRG 模式
 * ==================================================================
 *
 *  DRG 是"从 lower 线性走到 upper 再走回 lower"的斜坡发生器，可注入
 *  FTW / POW / ASF。速率 = SYSCLK/4 × pos_rate 步/秒 (近似)。
 */
void ad9910_drg_configure(const ad9910_drg_cfg_t *cfg)
{
    /* Reg 0x0B DRG Limit (64bit): [63:32]=upper, [31:0]=lower */
    uint64_t limit = ((uint64_t)cfg->upper << 32) | (uint64_t)cfg->lower;
    ad9910_write64(AD9910_REG_DRG_LIMIT, limit);

    /* Reg 0x0C DRG Step Size: [63:32]=decrement, [31:0]=increment */
    uint64_t step = ((uint64_t)cfg->decr_step << 32) | (uint64_t)cfg->incr_step;
    ad9910_write64(AD9910_REG_DRG_STEP, step);

    /* Reg 0x0D DRG Rate: [31:16]=negative slope rate, [15:0]=positive */
    uint32_t rate = ((uint32_t)cfg->neg_rate << 16) | (uint32_t)cfg->pos_rate;
    ad9910_write32(AD9910_REG_DRG_RATE, rate);

    /* CFR2: 使能 DRG + 目标 (+ no-dwell 位) */
    uint32_t cfr2 = AD9910_CFR2_BASE | AD9910_CFR2_DRG_ENABLE;
    switch (cfg->dest) {
    case AD9910_DRG_DEST_FTW: cfr2 |= AD9910_CFR2_DRG_DEST_FTW; break;
    case AD9910_DRG_DEST_POW: cfr2 |= AD9910_CFR2_DRG_DEST_POW; break;
    case AD9910_DRG_DEST_ASF: cfr2 |= AD9910_CFR2_DRG_DEST_ASF; break;
    }
    if (cfg->no_dwell_high) cfr2 |= AD9910_CFR2_DRG_NODWELL_HIGH;
    if (cfg->no_dwell_low)  cfr2 |= AD9910_CFR2_DRG_NODWELL_LOW;
    ad9910_write32(AD9910_REG_CFR2, cfr2);

    ad9910_io_update();
}

void ad9910_drg_disable(void)
{
    /* 回到 CFR2 base (DRG 关闭) */
    ad9910_write32(AD9910_REG_CFR2, AD9910_CFR2_BASE);
    ad9910_io_update();
}

/* ==================================================================
 *  OSK 键控
 * ================================================================== */
void ad9910_osk_enable(bool en)
{
    /* 保留当前 CFR1 里 RAM 相关位 (若在 RAM 模式) 太复杂；这里仅在单音模式
     * 使用 OSK，直接写 CFR1 = OSK_ENABLE | OSK_MANUAL */
    uint32_t cfr1 = en ? (AD9910_CFR1_OSK_ENABLE | AD9910_CFR1_OSK_MANUAL) : 0;
    ad9910_write32(AD9910_REG_CFR1, cfr1);
}

void ad9910_osk_key(bool on)
{
    if (on) AD9910_OSK_HI(); else AD9910_OSK_LO();
}

/* ==================================================================
 *  查询 & 调试
 * ================================================================== */
const ad9910_state_t *ad9910_get_state(void) { return &s_state; }

void ad9910_dump_state(void)
{
    UART_Printf("[ad9910] ============================\r\n");
    UART_Printf("  refclk    : %lu Hz\r\n", (unsigned long)AD9910_REFCLK_HZ);
    UART_Printf("  pll_n     : %u  → sysclk %lu Hz\r\n",
                (unsigned)AD9910_PLL_N, (unsigned long)AD9910_SYSCLK_HZ);
    UART_Printf("  pll_lock  : %s (assumed, SDO not wired)\r\n",
                s_state.pll_locked ? "yes" : "no");
    UART_Printf("  freq      : %.3f Hz  (FTW=0x%08lX)\r\n",
                s_state.f_hz, (unsigned long)s_state.last_ftw);
    UART_Printf("  amp01     : %.4f    (ASF=0x%04X /14bit)\r\n",
                (double)s_state.amp01, (unsigned)s_state.last_asf14);
    UART_Printf("  phase     : %.2f°   (POW=0x%04X)\r\n",
                (double)s_state.phase_deg, (unsigned)s_state.last_pow);
    UART_Printf("  profile   : %u\r\n", (unsigned)s_state.profile_sel);
    UART_Printf("  output    : %s\r\n", s_state.output_on ? "ON" : "OFF");
    UART_Printf("[ad9910] ============================\r\n");
}
