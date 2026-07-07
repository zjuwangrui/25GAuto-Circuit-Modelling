#ifndef __DRV_AD9910_H
#define __DRV_AD9910_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  AD9910 DDS 驱动
 * ===========================================================================
 *
 * 输入参考时钟 : 25 MHz 外部晶振
 * 内部 PLL     : 25MHz × 40 = 1 GHz SYSCLK
 * DAC 分辨率   : 14 bit（差分电流输出，需外接 balun + LPF）
 * 频率分辨率   : ≈ 0.233 Hz（1e9 / 2^32）
 * SPI          : SPI1，4 线 Mode 0，MSB first
 *
 * ---------------------------------------------------------------------------
 *  硬件连接（默认，可在 ad9910.c 里改宏）
 * ---------------------------------------------------------------------------
 *   AD9910 SDIO   → PA7  (SPI1 MOSI)
 *   AD9910 SDO    → PA6  (SPI1 MISO)      —— 4 线模式时才需要
 *   AD9910 SCLK   → PA5  (SPI1 SCK)
 *   AD9910 CSB    → PA4  (GPIO OUTPUT_PP)
 *   AD9910 RESET  → PB1  (GPIO OUTPUT_PP，高电平复位)
 *   AD9910 IO_UPDATE
 *       方案 A（默认）：接 MCU PB2  —— 写完寄存器 MCU 脉冲
 *       方案 B（板上焊到 CSB）：把 AD9910_IO_UPDATE_ON_CS 改成 1
 *
 * ---------------------------------------------------------------------------
 *  使用
 * ---------------------------------------------------------------------------
 *   ad9910_init();                       // 一次性：复位 + PLL 锁定 + 默认参数
 *   ad9910_set_freq_hz(1000000.0);       // 1 MHz 单音
 *   ad9910_set_amp01(0.5f);              // 幅度 50%
 *   ad9910_set_phase_deg(0.0f);
 *   ad9910_output_enable(true);
 *
 *   Terminal:
 *     dds.freq <hz>
 *     dds.amp  <0..1>
 *     dds.phase <deg>
 *     dds.on / dds.off
 *     dds.info
 * ===========================================================================
 */

#define AD9910_SYSCLK_HZ      1000000000UL   /* 1 GHz 内部 sysclk */

void   ad9910_init(void);

/* 单音输出参数设置（每次都会脉冲 IO_UPDATE 使其立即生效） */
void   ad9910_set_freq_hz  (double f_hz);
void   ad9910_set_amp01    (float  amp);      /* 0.0 ~ 1.0 → 14-bit ASF */
void   ad9910_set_phase_deg(float  deg);      /* 0 ~ 360   → 16-bit POW */

/* 一次原子设置频率+幅度+相位（内部只做一次 IO_UPDATE） */
typedef struct {
    double f_hz;
    float  amp01;
    float  phase_deg;
} ad9910_tone_t;
void   ad9910_set_tone(const ad9910_tone_t *t);

void   ad9910_output_enable(bool en);         /* 使能/关断 DAC 输出 */
void   ad9910_soft_reset   (void);            /* 脉冲 MASTER_RESET */

/* 当前配置读回（供 terminal / UI 显示） */
typedef struct {
    double   f_hz;
    float    amp01;
    float    phase_deg;
    bool     output_on;
    bool     pll_locked;
} ad9910_state_t;
const ad9910_state_t *ad9910_get_state(void);

#endif /* __DRV_AD9910_H */
