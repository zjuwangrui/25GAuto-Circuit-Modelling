/*
 * panel_ctrl.c —— 串口屏 <-> 业务模块 胶水层
 *
 * 只处理 4 个屏控件:
 *   1) 输出信号按钮 (0x2F) — 组合帧 04 11 2F [freq],[vpp] 00 FF → signal_out_set
 *   2) 学习按钮     (0x30) — 短触摸帧 FE 30 xx FF             → learning_start
 *   3) 推理按钮     (0x31) — 短触摸帧 FE 31 xx FF             → inference_start
 *   4) 滤波类型文本 (0x60) — 学习完成后 MCU 推送到屏
 *
 * ISR 上下文 (screen 回调) 只做最小工作: 存 pending 标志 + 参数.
 * 真正动作在 panel_ctrl_task 里做, 避开 ISR 里调 SPI/HAL_Delay 的坑.
 */

#include "module/panel_ctrl.h"
#include "drv/serial_screen.h"
#include "module/signal_out.h"
#include "module/learning.h"
#include "module/inference.h"
#include "bsp/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 *  ISR 与 task 之间的通信 (待办 flag)
 * ================================================================== */

static volatile bool     s_pending_output = false;
static volatile double   s_new_freq       = 0.0;
static volatile float    s_new_vpp        = 0.0f;

static volatile bool     s_pending_learn  = false;
static volatile bool     s_pending_infer  = false;

/* learning 状态跟踪 (完成时推一次滤波类型到屏, 不重复) */
static learn_state_t s_last_learn_state    = LEARN_IDLE;
static bool          s_learn_result_pushed = false;

/* ==================================================================
 *  ASCII → 数字 (支持负号 / 小数点)
 * ================================================================== */
static double ascii_slice_to_double(const uint8_t *s, uint8_t n)
{
    if (!s || n == 0) return 0.0;
    char tmp[16];
    uint8_t m = (n < sizeof(tmp) - 1) ? n : (uint8_t)(sizeof(tmp) - 1);
    memcpy(tmp, s, m);
    tmp[m] = '\0';
    return atof(tmp);
}

/* 从 "freq,vpp" 字符串里拆出两个数值.
 * 返回 true = 成功找到逗号且拆出两段, false = 格式错. */
static bool parse_freq_vpp(const uint8_t *s, uint8_t n, double *out_f, float *out_v)
{
    uint8_t comma_pos;
    for (comma_pos = 0; comma_pos < n; ++comma_pos) {
        if (s[comma_pos] == ',') break;
    }
    if (comma_pos == 0 || comma_pos >= n - 1) return false;

    *out_f = ascii_slice_to_double(s, comma_pos);
    *out_v = (float)ascii_slice_to_double(s + comma_pos + 1,
                                          (uint8_t)(n - comma_pos - 1));
    return true;
}

/* ==================================================================
 *  drv/serial_screen 回调 —— 在 ISR 上下文
 * ================================================================== */

static void on_touch(uint8_t d0, uint8_t d1)
{
    /* 短触摸帧 FE [d0] [d1] FF. d0 = 按钮 ID. */
    (void)d1;
    switch (d0) {
    case SCREEN_BTN_LEARN: s_pending_learn = true; break;
    case SCREEN_BTN_INFER: s_pending_infer = true; break;
    default: break;
    }
}

static void on_input(const uint8_t *p, uint8_t n)
{
    /* 长输入帧 04 11 [ctrl_id] [payload] 00 FF.
     * 目前只有一种: 输出信号按钮 (0x2F), payload = "freq,vpp" ASCII */
    if (n < 4) return;                    /* 至少 id + "0,0" */
    uint8_t ctrl_id = p[0];

    if (ctrl_id == SCREEN_BTN_OUTPUT_SIGNAL) {
        double f;
        float  v;
        if (parse_freq_vpp(p + 1, (uint8_t)(n - 1), &f, &v)) {
            s_new_freq       = f;
            s_new_vpp        = v;
            s_pending_output = true;
        }
    }
}

/* ==================================================================
 *  API
 * ================================================================== */

void panel_ctrl_init(void)
{
    screen_init();
    screen_on_touch(on_touch);
    screen_on_input(on_input);
}

void panel_ctrl_task(void)
{
    /* ---- 1. 输出信号 ---- */
    if (s_pending_output) {
        s_pending_output = false;
        UART_Printf("[panel] OUTPUT btn: signal_out_set(%.2f Hz, %.3f V)\r\n",
                    s_new_freq, (double)s_new_vpp);
        signal_out_set(s_new_freq, s_new_vpp);
    }

    /* ---- 2. 学习 ---- */
    if (s_pending_learn) {
        s_pending_learn = false;
        UART_Printf("[panel] LEARN btn\r\n");
        learning_start();
        s_learn_result_pushed = false;   /* 新一轮学习, 允许结果重新推屏 */
    }

    /* ---- 3. 推理 (无屏回推) ---- */
    if (s_pending_infer) {
        s_pending_infer = false;
        UART_Printf("[panel] INFER btn\r\n");
        inference_start();
    }

    /* ---- 4. 学习完成 → 只推滤波类型到屏 ---- */
    learn_state_t ls = learning_get_state();
    if (ls != s_last_learn_state) {
        s_last_learn_state = ls;
    }
    if (ls == LEARN_DONE && !s_learn_result_pushed) {
        const learn_result_t *r = learning_get_result();
        screen_set_text(SCREEN_TEXT_FILTER_TYPE, learning_filter_name(r->type));
        UART_Printf("[panel] push filter type -> %s\r\n",
                    learning_filter_name(r->type));
        s_learn_result_pushed = true;
    }
}
