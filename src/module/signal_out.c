/*
 * signal_out.c —— 目标输出信号反算, 补偿硬件后级 (×4 + H(s))
 *
 * H(s) = 5 / (1e-8·s² + 3e-4·s + 1)
 *
 * 对正弦稳态 (s = jω):
 *   |H(jω)| = 5 / sqrt( (1 - 1e-8·ω²)² + (3e-4·ω)² )
 *
 * 反算 (差分 Vpp):
 *   V_ad9910 = V_target / (4 · |H(jω)|)
 */

#include "module/signal_out.h"
#include "module/dds.h"
#include "module/ui.h"
#include "bsp/uart.h"
#include "bsp/lcd.h"
#include <math.h>
#include <stdio.h>

#ifndef DDS_FULL_SCALE_V
#define DDS_FULL_SCALE_V   1.0f
#endif

#define TWO_PI              6.28318530717958647692

/* H(s) 系数 (代码里改电路只用改这几行) */
#define H_S_COEF_S2         1e-8       /* s² 项 */
#define H_S_COEF_S1         3e-4       /* s¹ 项 */
#define H_S_DC_GAIN         5.0f       /* DC 增益 (常数项分子/分母) */

/* LCD 提示位置 (贴屏幕底部, 一行 15px 高) */
#define WARN_LCD_X          0
#define WARN_LCD_Y          225
#define WARN_LCD_W          320
#define WARN_LCD_H          15

/* -------------------------------------------------------------- */

float signal_out_H_mag(double freq_hz)
{
    double omega    = TWO_PI * freq_hz;
    double omega_sq = omega * omega;
    double real     = 1.0 - H_S_COEF_S2 * omega_sq;
    double imag     = H_S_COEF_S1 * omega;
    double denom    = sqrt(real * real + imag * imag);
    if (denom < 1e-12) denom = 1e-12;               /* 数值保护, 理论上不会 */
    return (float)((double)H_S_DC_GAIN / denom);
}

float signal_out_calc_ad9910_vpp(double freq_hz, float target_vpp)
{
    if (target_vpp < 0.0f) target_vpp = 0.0f;
    float total_gain = SIGNAL_OUT_AMP_GAIN * signal_out_H_mag(freq_hz);
    if (total_gain < 1e-6f) total_gain = 1e-6f;      /* 极高频防除零 */
    return target_vpp / total_gain;
}

/* -------------------------------------------------------------- */

static void lcd_clear_warning(void)
{
    LCD_FillRect(WARN_LCD_X, WARN_LCD_Y, WARN_LCD_W, WARN_LCD_H, BLACK);
}

static void lcd_show_warning(double freq_hz, float target_vpp, float need_ad9910)
{
    /* 一行红字提示: 目标 vpp 超硬件极限 */
    LCD_FillRect(WARN_LCD_X, WARN_LCD_Y, WARN_LCD_W, WARN_LCD_H, BLACK);
    LCD_DrawTextf(WARN_LCD_X, WARN_LCD_Y, RED, BLACK,
                  "!SIG OUT LIMIT: f=%.1fHz need DDS %.2fV > %.2fV",
                  freq_hz, (double)need_ad9910, (double)DDS_FULL_SCALE_V);
}

/* -------------------------------------------------------------- */

/* 内部状态: 最近一次 signal_out_set 的参数, 供 signal_out_task 打印用 */
static struct {
    bool     valid;         /* signal_out_set 至少被调过一次 */
    bool     last_ok;       /* 上次调用是否落到 AD9910 里 (未超限) */
    double   freq_hz;
    float    target_vpp;
    float    h_mag;
    float    need_ad9910;
} s_state = {
    .valid = false, .last_ok = false,
    .freq_hz = 0.0, .target_vpp = 0.0f,
    .h_mag = 0.0f, .need_ad9910 = 0.0f,
};

/* -------------------------------------------------------------- */

void signal_out_init(void)
{


}

bool signal_out_set(double freq_hz, float target_vpp)
{

    float h_mag       = signal_out_H_mag(freq_hz);
    float total_gain  = SIGNAL_OUT_AMP_GAIN * h_mag;
    float need_ad9910 = target_vpp / total_gain;

    /* 缓存到状态里, signal_out_task 会定期打印 */
    s_state.valid       = true;
    s_state.freq_hz     = freq_hz;
    s_state.target_vpp  = target_vpp;
    s_state.h_mag       = h_mag;
    s_state.need_ad9910 = need_ad9910;

    UART_Printf("[sigout] target f=%.2fHz vpp=%.3fV | |H|=%.3f gain(x%.2fH)=%.3f "
                "-> AD9910 vpp=%.4fV\r\n",
                freq_hz, (double)target_vpp,
                (double)h_mag, (double)SIGNAL_OUT_AMP_GAIN,
                (double)total_gain, (double)need_ad9910);

    if (need_ad9910 > DDS_FULL_SCALE_V) {
        UART_Printf("[sigout] !! LIMIT: need %.4fV > DDS full-scale %.2fV, "
                    "NOT outputting.\r\n",
                    (double)need_ad9910, (double)DDS_FULL_SCALE_V);
        lcd_show_warning(freq_hz, target_vpp, need_ad9910);
        dds_tone_sine(freq_hz, need_ad9910, 0.0f);
        s_state.last_ok = false;
        return false;
    }

    /* 反算成功, 清 LCD 提示后实际下发 */
    lcd_clear_warning();
    dds_tone_sine(freq_hz, need_ad9910, 0.0f);
    s_state.last_ok = true;
    return true;
}

/* -------------------------------------------------------------- */

void signal_out_task(void)
{
    if (!s_state.valid) {
        UART_Printf("[sigout] task: no signal set yet\r\n");
        return;
    }
    UART_Printf("[sigout] f=%.2fHz  target_vpp=%.3fV  |H|=%.3f  AD9910_vpp=%.4fV  %s\r\n",
                s_state.freq_hz,
                (double)s_state.target_vpp,
                (double)s_state.h_mag,
                (double)s_state.need_ad9910,
                s_state.last_ok ? "OK" : "OVER-LIMIT");
}
