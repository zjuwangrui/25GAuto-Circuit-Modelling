#ifndef __DRV_SERIAL_SCREEN_H
#define __DRV_SERIAL_SCREEN_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  广州大彩串口屏驱动 (V5.1 指令集)
 *  参考文档: resources/大彩串口屏指令集V5.1.pdf
 * ===========================================================================
 *
 *  串口: UART2  PA2(TX) / PA3(RX)  115200 8N1
 *
 * ---------------------------------------------------------------------------
 *  屏 → MCU 上报帧 (统一 CMD = B1 11, 但两种类型帧尾不同)
 * ---------------------------------------------------------------------------
 *
 *    EE  B1 11  [Screen_id:2B_BE]  [Ctrl_id:2B_BE]  [Ctrl_type:1B]  [payload]  [尾部]
 *                                                     ↑↑↑↑
 *                                                     决定 payload 格式和尾部
 *
 *  控件类型 (Ctrl_type):
 *      0x10  按钮控件  → payload = Subtype(1B) + Status(1B)
 *                        Subtype: 0x00=基本按钮, 0x01=开关式
 *                        Status : 0x00=松开, 0x01=按下
 *                        帧总长度 **固定 10 字节, 无 FF FC FF FF 帧尾**
 *                        (原因: VisualTFT 里按钮自定义指令不能含 0xFF,
 *                         所以采用固定长度识别, 不用 footer.)
 *
 *      0x11  文本控件  → payload = 变长 ASCII/GB2312, 末尾 0x00 终止
 *                        帧尾: FF FC FF FF (大彩标准, 文本控件自动加)
 *
 *  例:
 *    频率输入框 (ctrl=4) 输入 "1000":
 *      EE B1 11 00 00 00 04 11 31 30 30 30 00 FF FC FF FF     (17 字节)
 *
 *    输出信号按钮 (ctrl=11) 按下:
 *      EE B1 11 00 00 00 0B 10 00 01                          (10 字节, 无尾)
 *      ↑                    ↑↑↑↑↑                            Subtype=00, Status=01
 *
 * ---------------------------------------------------------------------------
 *  MCU → 屏 (更新文本控件, CMD = B1 10)
 * ---------------------------------------------------------------------------
 *
 *    EE  B1 10  [Screen_id:2B]  [Ctrl_id:2B]  [ASCII strings]  FF FC FF FF
 *
 *  MCU 主要用来写显示文本 (例如学习完成后写滤波类型).
 * ===========================================================================
 */

/* 上层回调: 每收到一个完整的 EE B1 11 ... FF FC FF FF 帧, 就调一次这个.
 *   screen_id, ctrl_id 都是 16-bit 大端组好的值.
 *   ctrl_type  = 0x10 (按钮) / 0x11 (文本) / ...
 *   payload    是 [Ctrl_type] 之后到帧尾之间的数据 (文本类的 0x00 终止符已剥掉).
 *   payload_len 可能为 0 (帧无 payload). */
typedef void (*screen_frame_cb_t)(uint16_t screen_id,
                                  uint16_t ctrl_id,
                                  uint8_t  ctrl_type,
                                  const uint8_t *payload,
                                  uint8_t  payload_len);

/* ---------- 生命周期 ---------- */

void screen_init(void);                     /* 内部调用 MX_USART2_UART_Init */
void screen_on_frame(screen_frame_cb_t cb); /* 注册上行帧回调 (中断上下文) */

/* ---------- 发送 ---------- */

/* 更新指定文本控件的内容 (MCU → 屏). 帧格式:
 *   EE B1 10 [Screen_id=0] [Ctrl_id] [ASCII 字节串] FF FC FF FF */
void screen_set_text(uint16_t ctrl_id, const char *ascii);

/* 直接发一段原始字节 (用于自定义指令) */
void screen_send_raw(const uint8_t *data, uint16_t len);

#endif /* __DRV_SERIAL_SCREEN_H */
