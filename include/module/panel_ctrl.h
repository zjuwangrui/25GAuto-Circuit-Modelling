#ifndef __MODULE_PANEL_CTRL_H
#define __MODULE_PANEL_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  module/panel_ctrl —— 串口屏与业务模块之间的胶水层
 * ===========================================================================
 *
 *  一共 4 个屏控件:
 *      1) 输出信号按钮   Screen → MCU   组合帧, 携带 freq/vpp
 *      2) 学习按钮       Screen → MCU   短触摸帧, 无 payload
 *      3) 推理按钮       Screen → MCU   短触摸帧, 无 payload
 *      4) 滤波类型文本   MCU → Screen   学习完成后 MCU 推送
 *
 *  屏事件 → 业务动作:
 *      "输出信号" 按钮 → 组合帧 04 11 [0x2F] [freq_ascii] ',' [vpp_ascii] 00 FF
 *                       MCU 解出 freq/vpp → signal_out_set(freq, vpp)
 *      "学习"   按钮 → 短触摸帧 FE [0x30] event FF → learning_start()
 *      "推理"   按钮 → 短触摸帧 FE [0x31] event FF → inference_start()
 *
 *      注: 屏上的频率/电压输入框在输入过程中不主动发数据, 输入值存在屏本地,
 *          用户点 "输出信号" 时屏一次性打包发出.
 *
 *  业务状态 → 屏显示:
 *      learning DONE   → 屏显示 学到的滤波类型 (推一个文本, 就这个)
 *      inference       → 不回推屏
 *      signal_out_set  → 不回推屏
 *
 *  注意: 屏 UART 中断只做 "存待办 flag", 真正的动作在 panel_ctrl_task 里做,
 *        避免在 ISR 上下文里调 SPI/HAL_Delay.
 *
 * ---------------------------------------------------------------------------
 *  控件 ID 分配 (占位, 与上位机 VisualTFT 工程对齐后改这里)
 * ---------------------------------------------------------------------------
 */

/* --- 屏 → MCU: 触摸按钮 ID --- */
#ifndef SCREEN_BTN_OUTPUT_SIGNAL
#define SCREEN_BTN_OUTPUT_SIGNAL    0x2F     /* 组合帧: 04 11 [2F] [freq],[vpp] 00 FF */
#endif
#ifndef SCREEN_BTN_LEARN
#define SCREEN_BTN_LEARN            0x30     /* 短触摸帧: FE [30] [event] FF */
#endif
#ifndef SCREEN_BTN_INFER
#define SCREEN_BTN_INFER            0x31     /* 短触摸帧: FE [31] [event] FF */
#endif

/* --- MCU → 屏: 文本控件 ID --- */
#ifndef SCREEN_TEXT_FILTER_TYPE
#define SCREEN_TEXT_FILTER_TYPE     0x60     /* LOWPASS / HIGHPASS / ... */
#endif

/* ==========================================================================
 *  API
 * ========================================================================== */
void panel_ctrl_init(void);      /* 内部会 screen_init + 注册回调 */
void panel_ctrl_task(void);      /* 注册到调度器 (100~200ms 即可) */

#endif /* __MODULE_PANEL_CTRL_H */
