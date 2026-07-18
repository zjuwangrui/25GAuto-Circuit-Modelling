#ifndef __DRV_AD9910_H
#define __DRV_AD9910_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  AD9910 DDS 驱动 —— 硬件层 (drv 层)
 * ===========================================================================
 *
 * 职责边界：
 *   本层 = "AD9910 寄存器动作 + SPI/GPIO 抽象"。只暴露"原子的寄存器/引脚
 *   操作"，不做业务模式选择。业务级模式 (单音 / 任意波 / 扫频 / 跳频 / OSK)
 *   请写到 module/dds 里，用本层的原子接口拼出来。
 *
 * ---------------------------------------------------------------------------
 *  时钟树 (与 resources/AD9910Demo v1.1 对齐)
 * ---------------------------------------------------------------------------
 *      外部 TCXO 40 MHz  →  PLL ×25  →  SYSCLK 1 GHz  →  内部 DAC/CIC
 *
 *      频率分辨率 Δf = SYSCLK / 2^32 ≈ 0.233 Hz
 *      推荐输出上限 ≈ 400 MHz (超 0.4·SYSCLK 后 sinc 衰减很大)
 *
 *      如果你的板子是 25 MHz TCXO：把 AD9910_REFCLK_HZ 改成 25000000，
 *      AD9910_PLL_N 改成 40 即可；SYSCLK 仍 = 1 GHz，FTW 换算表达式保持不变。
 *
 * ---------------------------------------------------------------------------
 *  硬件连接 (STM32F103VE)
 * ---------------------------------------------------------------------------
 *      功能              STM32 引脚      AD9910 模块丝印        必要性 (单音模式)
 *      ----              -----------     ---------------        ----------------
 *      SPI1 SCK          PA5             SCK                    必接
 *      SPI1 MOSI         PA7             SDIO                   必接
 *      SPI1 MISO         PA6             SDO                    不接 (模块未引出)
 *      CS                PA4             CSB                    必接
 *      RESET             PC3             RST (MASTER_RESET)     必接
 *      IO_UPDATE         PC4             IOUP                   必接
 *
 *      —— 下面三根 datasheet 明确要求 "不用则接地" ——
 *      PWR (拉低)        PC0             PWR (EXT_PWR_DWN)      必接 (拉高 = 芯片关机)
 *      DPH (拉低)        PC1             DPH (DRHOLD)           必接 (拉高 = DRG 冻结)
 *      DRC (拉低)        PC2             DRC (DRCTL)            必接 (悬空会随机)
 *
 *      —— 下面几根仅特定模式用，单音下 MCU 输出低即可 (不必接线) ——
 *      PROFILE[0]        PB12            PF0                    Mode 4 (跳频) 用
 *      PROFILE[1]        PB13            PF1
 *      PROFILE[2]        PB14            PF2
 *      OSK               PB15            OSK                    Mode 5 (键控) 用
 *
 *      GND               GND             GND                    必接 (共地)
 *      —                 —               DC-5V                  模块单独 5V 供电
 *
 *      若板子把 IO_UPDATE 硬短接到 CSB，把 AD9910_IO_UPDATE_ON_CS 改成 1
 *
 * ---------------------------------------------------------------------------
 *  典型 SPI 帧 (每次寄存器访问 = 一个 CSB 低-高 周期)
 * ---------------------------------------------------------------------------
 *      CSB↓ | Instr(1B) | Data(1..8B) | CSB↑
 *        Instr = [R/W#:1] [_:2=00] [Addr:5]
 *        Data  = 大端 MSB first；不同寄存器长度不同 (见 REGLEN 表)
 *        写完可选 IO_UPDATE 脉冲以使数据生效
 * ===========================================================================
 */

/* ==================================================================
 *  时钟/PLL 配置 —— 改板子改这里
 * ================================================================== */
#define AD9910_REFCLK_HZ           40000000UL                              /* 外部参考 */
#define AD9910_PLL_N               25U                                     /* PLL 倍频 */
#define AD9910_SYSCLK_HZ           ((uint64_t)AD9910_REFCLK_HZ * AD9910_PLL_N)  /* 1 GHz */

/* IO_UPDATE 是否与 CSB 复用 (板子上焊到一起的情况) */
#define AD9910_IO_UPDATE_ON_CS     0

/* ==================================================================
 *  寄存器地址 (Datasheet Table 15)
 * ================================================================== */
#define AD9910_REG_CFR1            0x00   /* 32-bit */
#define AD9910_REG_CFR2            0x01   /* 32-bit */
#define AD9910_REG_CFR3            0x02   /* 32-bit */
#define AD9910_REG_AUX_DAC         0x03
#define AD9910_REG_IO_UPD_RATE     0x04
#define AD9910_REG_FTW             0x07   /* 32-bit (仅单音无 profile 时用) */
#define AD9910_REG_POW             0x08   /* 16-bit */
#define AD9910_REG_ASF             0x09   /* 32-bit (含 ASF 步进与 14bit 幅度) */
#define AD9910_REG_MULTICHIP_SYNC  0x0A
#define AD9910_REG_DRG_LIMIT       0x0B   /* 64-bit: [63:32]=upper, [31:0]=lower */
#define AD9910_REG_DRG_STEP        0x0C   /* 64-bit: [63:32]=decr,  [31:0]=incr */
#define AD9910_REG_DRG_RATE        0x0D   /* 32-bit: [31:16]=neg,   [15:0]=pos  */
#define AD9910_REG_PROFILE0        0x0E   /* 64-bit,单音: [63:48]=ASF [47:32]=POW [31:0]=FTW */
#define AD9910_REG_PROFILE(n)      (uint8_t)(AD9910_REG_PROFILE0 + (n))    /* n=0..7 */
#define AD9910_REG_RAM             0x16   /* 32-bit/字, 起始地址由 profile 决定 */

/* ==================================================================
 *  CFR 位掩码 (只列了本工程会动的位，其余位保持 base 值)
 * ================================================================== */

/* CFR1 (0x00) */
#define AD9910_CFR1_RAM_ENABLE         (1UL << 31)
#define AD9910_CFR1_RAM_DEST_FTW       (0UL << 29)
#define AD9910_CFR1_RAM_DEST_POW       (1UL << 29)
#define AD9910_CFR1_RAM_DEST_ASF       (2UL << 29)
#define AD9910_CFR1_RAM_DEST_POLAR     (3UL << 29)      /* 幅相同时 (arb waveform 常用) */
#define AD9910_CFR1_OSK_ENABLE         (1UL <<  9)
#define AD9910_CFR1_OSK_MANUAL         (1UL <<  8)

/* CFR2 (0x01) — Base 位 (匹配 example 语义: 匹配延迟 + sync clk + ASF 来自 profile) */
#define AD9910_CFR2_BASE               0x01400820UL
#define AD9910_CFR2_DRG_ENABLE         (1UL << 19)
#define AD9910_CFR2_DRG_DEST_FTW       (0UL << 20)
#define AD9910_CFR2_DRG_DEST_POW       (1UL << 20)
#define AD9910_CFR2_DRG_DEST_ASF       (2UL << 20)
#define AD9910_CFR2_DRG_NODWELL_HIGH   (1UL << 18)
#define AD9910_CFR2_DRG_NODWELL_LOW    (1UL << 17)

/* CFR3 (0x02) — PLL 配置。40 MHz × 25 = 1 GHz。与 example AD9910_Cfg.c 一致。 */
#define AD9910_CFR3_VALUE              0x050F4132UL     /* N=25, VCO=5, ICP=3, PLL_EN=1 */

/* ==================================================================
 *  数据类型
 * ================================================================== */

/* 单音三要素 */
typedef struct {
    double f_hz;         /* 频率  Hz    (0 ~ ~0.4·SYSCLK)              */
    float  amp01;        /* 幅度  0..1  (对应 ASF 0..0x3FFF)           */
    float  phase_deg;    /* 相位  0..360° (对应 POW 0..0xFFFF)         */
} ad9910_tone_t;

/* RAM playback profile 参数 (对应 profile 寄存器在 RAM 模式下的解读) */
/* AD9910 datasheet Table 13 RAM Operating Modes (CFR-profile bits [2:0]) */
typedef enum {
    AD9910_RAM_MODE_DIRECT_SWITCH   = 0,   /* 000: direct switch (PROFILE 引脚选一个字) */
    AD9910_RAM_MODE_RAMP_UP         = 1,   /* 001: ramp-up (start→end 走一遍) */
    AD9910_RAM_MODE_BIDIRECTIONAL   = 2,   /* 010: bidirectional ramp (start→end→start 一次) */
    AD9910_RAM_MODE_CONT_BIDIR      = 3,   /* 011: continuous BIDIRECTIONAL (连续双向, 非回文数据频率会减半) */
    AD9910_RAM_MODE_CONT_RECIRC     = 4,   /* 100: continuous RECIRCULATE (连续单向环回, 常规选这个) */
    /* 保留旧名字以避免破坏依赖 (但语义已按 datasheet 修正) */
    AD9910_RAM_MODE_CONTINUOUS_RAMP  = AD9910_RAM_MODE_CONT_BIDIR,   /* 曾用名, 实际是双向 */
    AD9910_RAM_MODE_CONTINUOUS_BIDIR = AD9910_RAM_MODE_CONT_RECIRC,  /* 曾用名, 实际是单向 */
} ad9910_ram_mode_t;


typedef struct {
    uint16_t          start_addr;   /* 0..1023 */
    uint16_t          end_addr;     /* 0..1023 */
    uint16_t          rate_divider; /* RAM 地址步进速率 = SYSCLK/4/(rate_divider+1)，16bit */
    ad9910_ram_mode_t mode;
    bool              no_dwell_high;
    bool              zero_crossing;
} ad9910_ram_profile_t;

/* DRG (Digital Ramp Generator) 目标 */
typedef enum {
    AD9910_DRG_DEST_FTW = 0,
    AD9910_DRG_DEST_POW = 1,
    AD9910_DRG_DEST_ASF = 2,
} ad9910_drg_dest_t;

typedef struct {
    ad9910_drg_dest_t dest;
    uint32_t          lower;         /* 32-bit 起始值 (FTW/POW 左移到 32bit / ASF 左移) */
    uint32_t          upper;         /* 32-bit 终止值 */
    uint32_t          incr_step;
    uint32_t          decr_step;
    uint16_t          pos_rate;      /* 正向速率 (SYSCLK/4 计数) */
    uint16_t          neg_rate;      /* 反向速率 */
    bool              no_dwell_high; /* 到达上限立即回底                        */
    bool              no_dwell_low;  /* 到达下限立即回顶 (与 high 组合 = 单斜坡) */
} ad9910_drg_cfg_t;

/* 内部状态回读结构 */
typedef struct {
    double   f_hz;
    float    amp01;
    float    phase_deg;
    uint32_t last_ftw;
    uint16_t last_asf14;
    uint16_t last_pow;
    bool     pll_locked;    /* 由 ad9910_init 里 CFR3 读回校验设置 */
    bool     output_on;
    uint8_t  profile_sel;   /* 0..7 */
    uint32_t cfr3_readback; /* ad9910_init 里读回的 CFR3，用于事后调试 */
} ad9910_state_t;

/* ==================================================================
 *  生命周期
 * ================================================================== */
void ad9910_init      (void);   /* SPI+GPIO+复位+CFR+PLL锁定+默认关输出 */
void ad9910_soft_reset(void);   /* 单独脉冲 MASTER_RESET */

/* ==================================================================
 *  低层动作 —— 别在业务里直接调，走 module/dds
 * ================================================================== */
void ad9910_io_update(void);
void ad9910_write_reg(uint8_t addr, const uint8_t *data, uint8_t len);          /* 写后自动 io_update */
void ad9910_write_reg_noupdate(uint8_t addr, const uint8_t *data, uint8_t len); /* 批量写用 */
void ad9910_write32(uint8_t addr, uint32_t v);
void ad9910_write16(uint8_t addr, uint16_t v);
void ad9910_write64(uint8_t addr, uint64_t v);                                  /* profile / DRG 用 */

/* 4 线 SPI 读回 (要求 CFR1 bit1 SDIO_INPUT_ONLY = 1，ad9910_init 已经设好) */
void     ad9910_read_reg(uint8_t addr, uint8_t *buf, uint8_t len);
uint32_t ad9910_read32  (uint8_t addr);

/* 简单的 PLL 锁定判定：ad9910_init 里读回 CFR3 = 写入值 → true。
 * 严格来说这只证明 SPI 通信 + 寄存器保留，不能 100% 保证 PLL 已锁，
 * 但 40MHz 晶振 + 正确 CFR3 下 PLL 一般都能锁上。*/
bool ad9910_pll_is_locked(void);

/* ==================================================================
 *  参数换算 (公开给业务层用)
 * ================================================================== */
uint32_t ad9910_freq_to_ftw   (double f_hz);
uint16_t ad9910_amp01_to_asf  (float  amp01);   /* 14-bit  0..0x3FFF */
uint16_t ad9910_phase_deg_to_pow(float deg);    /* 16-bit  0..0xFFFF */

/* ==================================================================
 *  Profile & 单音
 * ================================================================== */
/* 一次原子写入 profile n 的 {ASF, POW, FTW}。写完自动 io_update。
 * 单音模式下 profile 0 即工作 profile。*/
void ad9910_profile_write(uint8_t n, uint16_t asf14, uint16_t pow16, uint32_t ftw);

/* 用引脚 PROFILE[2:0] 切换活动 profile (0..7)。跳频用。*/
void ad9910_profile_select(uint8_t n);

/* 单音三要素快捷接口 (只操作 profile 0，模块自动记录 s_state)。*/
void ad9910_set_freq_hz  (double f_hz);
void ad9910_set_amp01    (float  amp01);
void ad9910_set_phase_deg(float  deg);
void ad9910_set_tone     (const ad9910_tone_t *t);

/* ==================================================================
 *  RAM 模式 (任意波形)
 *
 *  用法：
 *    1) ad9910_ram_write(0, samples, N);
 *    2) ad9910_ram_profile_config(0, &prof);   // 把 profile0 用作 RAM playback 参数
 *    3) ad9910_ram_enable(AD9910_CFR1_RAM_DEST_POLAR);
 *    4) ad9910_io_update();
 * ================================================================== */
void ad9910_ram_write(uint16_t start_addr, const uint32_t *data, uint16_t n);
void ad9910_ram_profile_config(uint8_t profile_n, const ad9910_ram_profile_t *cfg);
void ad9910_ram_enable(uint32_t dest_mask);   /* 传 AD9910_CFR1_RAM_DEST_XXX 之一 */
void ad9910_ram_disable(void);                /* 回单音模式 */

/* ==================================================================
 *  DRG 模式 (线性扫频/斜坡)
 * ================================================================== */
void ad9910_drg_configure(const ad9910_drg_cfg_t *cfg);   /* 写 CFR2 + reg 0x0B/0C/0D */
void ad9910_drg_disable(void);
/* DRHOLD/DRCTL 引脚在本板未接，若接了另加 wrapper */

/* ==================================================================
 *  OSK 键控
 * ================================================================== */
void ad9910_osk_enable(bool en);        /* CFR1 里使能 OSK + manual */
void ad9910_osk_key   (bool on);        /* 通过 OSK 引脚打开/关闭输出 */

/* ==================================================================
 *  杂项 & 调试
 * ================================================================== */
void ad9910_output_enable(bool en);              /* 通过 ASF=0 实现"静音" */
const ad9910_state_t *ad9910_get_state(void);
void ad9910_dump_state(void);                    /* 通过 UART_Printf 打印当前寄存器摘要 */

#endif /* __DRV_AD9910_H */
