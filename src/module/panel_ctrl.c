/*
 * panel_ctrl.c —— 串口屏 ↔ 业务模块胶水层 (V5.1 协议)
 *
 * 屏 → MCU 上报帧统一格式:
 *   EE B1 11 [Screen_id:2B] [Ctrl_id:2B] [Ctrl_type:1B] [payload] FF FC FF FF
 *
 * 本模块只做:
 *   ISR 上下文: 组好帧后按 Ctrl_id 分发, 只置 flag / 缓存参数, 不做 SPI/ADC
 *   task 上下文 (50 ms): 检查 flag, 调对应业务模块
 *
 * 控件表 (与 panel_ctrl.h 里 SCREEN_CTRL_* 宏一致):
 *   4  文本 —— 频率输入
 *   7  文本 —— 电压输入
 *   11 按钮 —— 经模型输出   (M2, signal_out_set)
 *   40 按钮 —— 信号输出模式 (M1, dds_tone_sine 直调)
 *   50 按钮 —— 学习模式     (M3, learning_start)
 *   51 按钮 —— 推理模式     (M4, inference_start)
 *   52 按钮 —— 停止         (learning_stop + inference_stop + dds_stop)
 */

#include "module/panel_ctrl.h"
#include "drv/serial_screen.h"
#include "module/signal_out.h"
#include "module/dds.h"
#include "module/learning.h"
#include "module/inference.h"
#include "bsp/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 *  ISR 与 task 之间: 待办 flag + 参数缓存
 * ================================================================== */
static volatile bool     s_pending_output     = false;
static volatile bool     s_pending_raw_output = false;
static volatile bool     s_pending_learn      = false;
static volatile bool     s_pending_infer      = false;
static volatile bool     s_pending_stop       = false;

static volatile double   s_last_freq_v        = 1000.0;    /* 默认 1kHz */
static volatile float    s_last_vpp_v         = 1.0f;      /* 默认 1V   */

/* 学习结果推送屏显 (由 task 观察 learning_get_state 变化触发) */
static learn_state_t     s_prev_learn_state   = LEARN_IDLE;

/* ==================================================================
 *  ASCII → 数字
 * ================================================================== */
static double parse_ascii_double(const uint8_t *s, uint8_t n)
{
    if (!s || n == 0) return 0.0;
    char tmp[16];
    uint8_t m = (n < sizeof(tmp) - 1) ? n : (uint8_t)(sizeof(tmp) - 1);
    memcpy(tmp, s, m);
    tmp[m] = '\0';
    return atof(tmp);
}

/* ==================================================================
 *  按钮帧共享检查: 是否 "按下" 事件
 *   payload = [Subtype:1B, Status:1B], Status=0x01 = 按下
 * ================================================================== */
static inline bool is_button_press(uint8_t ctrl_type,
                                   const uint8_t *payload, uint8_t plen)
{
    return (ctrl_type == 0x10) && (plen >= 2) &&
           (payload[plen - 1] == 0x01);
}

/* ==================================================================
 *  drv/serial_screen 帧回调 (中断上下文)
 * ================================================================== */
static void on_frame(uint16_t screen_id, uint16_t ctrl_id, uint8_t ctrl_type,
                     const uint8_t *payload, uint8_t payload_len)
{
    (void)screen_id;

    switch (ctrl_id) {
    case SCREEN_CTRL_FREQ_INPUT:
        s_last_freq_v = parse_ascii_double(payload, payload_len);
        break;

    case SCREEN_CTRL_VPP_INPUT:
        s_last_vpp_v = (float)parse_ascii_double(payload, payload_len);
        break;

    case SCREEN_CTRL_OUTPUT_BTN:
        if (is_button_press(ctrl_type, payload, payload_len))
            s_pending_output = true;
        break;

    case SCREEN_CTRL_RAW_OUTPUT_BTN:
        if (is_button_press(ctrl_type, payload, payload_len))
            s_pending_raw_output = true;
        break;

    case SCREEN_CTRL_LEARN_BTN:
        if (is_button_press(ctrl_type, payload, payload_len))
            s_pending_learn = true;
        break;

    case SCREEN_CTRL_INFER_BTN:
        if (is_button_press(ctrl_type, payload, payload_len))
            s_pending_infer = true;
        break;

    case SCREEN_CTRL_STOP_BTN:
        if (is_button_press(ctrl_type, payload, payload_len))
            s_pending_stop = true;
        break;

    default:
        break;
    }
}

/* ==================================================================
 *  API
 * ================================================================== */
void panel_ctrl_init(void)
{
    screen_init();
    screen_on_frame(on_frame);
    s_prev_learn_state = LEARN_IDLE;
}

/* 每 50 ms 跑一次 —— 处理屏事件 + 观察学习状态变化推屏 */
void panel_ctrl_task(void)
{
    /* --- 屏事件分发 --- */
    if (s_pending_output) {
        s_pending_output = false;
        double f = s_last_freq_v;
        float  v = s_last_vpp_v;
        UART_Printf("[panel] M2 OUTPUT_H: signal_out_set(%.2f Hz, %.3f V)\r\n",
                    f, (double)v);
        signal_out_set(f, v);
    }

    if (s_pending_raw_output) {
        s_pending_raw_output = false;
        double f = s_last_freq_v;
        UART_Printf("[panel] M1 OUTPUT_RAW: dds_tone_sine(%.2f Hz, 0.6 V)\r\n", f);
        dds_tone_sine(f, 0.6f, 0.0f);
    }

    if (s_pending_learn) {
        s_pending_learn = false;
        UART_Printf("[panel] M3 LEARN: learning_start()\r\n");
        if (!learning_start())
            UART_Printf("[panel] learning already running\r\n");
    }

    if (s_pending_infer) {
        s_pending_infer = false;
        UART_Printf("[panel] M4 INFER: inference_start()\r\n");
        if (!inference_start())
            UART_Printf("[panel] inference start failed (no model?)\r\n");
    }

    if (s_pending_stop) {
        s_pending_stop = false;
        UART_Printf("[panel] STOP: learning/inference/dds_stop\r\n");
        learning_stop();
        inference_stop();
        dds_stop();
    }

    /* --- 学习状态变化观察: RUNNING → DONE 时推屏 --- */
    learn_state_t st = learning_get_state();
    if (st != s_prev_learn_state) {
        if (st == LEARN_DONE) {
            const learn_result_t *r = learning_get_result();
            const char *name = learning_filter_name(r->type);
            double f0 = (r->n_params >= 1)
                          ? (double)r->params[0] / 6.28318530717958
                          : 0.0;
            UART_Printf("[panel] LEARN done -> %s, f0=%.1f Hz\r\n", name, f0);
            /* TODO: 用 screen_update_text() 把 "name/f0" 推到屏文本控件 */
        } else if (st == LEARN_FAILED) {
            UART_Printf("[panel] LEARN failed\r\n");
        }
        s_prev_learn_state = st;
    }
}
