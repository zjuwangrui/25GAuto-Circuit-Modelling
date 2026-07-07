#ifndef __UART_H
#define __UART_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define UART_PRINTF_BUF_SZ 256U

/* 外设句柄 */
extern UART_HandleTypeDef huart1;  /* USART1 — 调试口  PA9/PA10   */
extern UART_HandleTypeDef huart3;  /* USART3 — ESP8266 PB10/PB11  */

/* CubeMX 风格初始化函数 */
void MX_USART1_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USART1 调试工具函数 */
void              UART_SendByte(uint8_t byte);
void              UART_SendString(const char *str);
void              UART_SendBytes(const uint8_t *data, uint16_t len);
int               UART_Printf(const char *fmt, ...);
uint8_t           UART_ReceiveByte(uint32_t timeout_ms);
HAL_StatusTypeDef UART_ReceiveBytes(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

#endif /* __UART_H */
