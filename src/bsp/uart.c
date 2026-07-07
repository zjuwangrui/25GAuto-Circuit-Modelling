#include "bsp/uart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* esp8266 中断处理入口 —— 弱符号缺省实现：
 * 未链接 drv/esp8266.c 时使用此空壳，避免链接错误。 */
__attribute__((weak)) void ESP_UART_IRQHandler(void) {}

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

static char _printf_buf[UART_PRINTF_BUF_SZ];

/* ------------------------------------------------------------------ */
/*  MX_USART1_UART_Init — 调试口  PA9(TX) / PA10(RX)  115200 8N1     */
/*  仅用于发送（printf 调试）；不开 RX 中断。                         */
/*  需要收字符自己 poll UART_ReceiveByte() 即可。                     */
/* ------------------------------------------------------------------ */
void MX_USART1_UART_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = GPIO_PIN_9;          /* PA9  TX */
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin   = GPIO_PIN_10;         /* PA10 RX（保留，需要时可轮询） */
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        while (1);
}

/* ------------------------------------------------------------------ */
/*  MX_USART3_UART_Init — ESP8266  PB10(TX) / PB11(RX)  115200 8N1   */
/* ------------------------------------------------------------------ */
void MX_USART3_UART_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = GPIO_PIN_10;         /* PB10 TX */
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin   = GPIO_PIN_11;         /* PB11 RX */
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
        while (1);

    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/* ------------------------------------------------------------------ */
/*  USART1 调试工具函数（TX 阻塞，任何模块可直接调用做打印）           */
/* ------------------------------------------------------------------ */
void UART_SendByte(uint8_t byte)
{
    HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
}

void UART_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1, (const uint8_t *)str, (uint16_t)strlen(str), HAL_MAX_DELAY);
}

void UART_SendBytes(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
}

int UART_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(_printf_buf, sizeof(_printf_buf), fmt, args);
    va_end(args);
    if (len > 0)
        UART_SendBytes((const uint8_t *)_printf_buf, (uint16_t)len);
    return len;
}

uint8_t UART_ReceiveByte(uint32_t timeout_ms)
{
    uint8_t b = 0;
    HAL_UART_Receive(&huart1, &b, 1, timeout_ms);
    return b;
}

HAL_StatusTypeDef UART_ReceiveBytes(uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    return HAL_UART_Receive(&huart1, buf, len, timeout_ms);
}

/* ------------------------------------------------------------------ */
/*  统一 HAL RxCplt 回调（当前只服务 USART3 → ESP8266）                */
/* ------------------------------------------------------------------ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        ESP_UART_IRQHandler();
    }
}
