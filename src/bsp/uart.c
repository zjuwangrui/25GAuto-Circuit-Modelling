#include "bsp/uart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* esp8266 中断处理入口 —— 弱符号缺省实现：
 * 未链接 drv/esp8266.c 时使用此空壳，避免链接错误。 */
__attribute__((weak)) void ESP_UART_IRQHandler(void) {}

/* 串口屏 (USART2) 单字节 RX 回调 —— 弱符号缺省实现：
 * 未链接 drv/serial_screen.c 时安全无副作用. */
__attribute__((weak)) void SCREEN_UART_IRQHandler(uint8_t byte) { (void)byte; }

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
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
/*  MX_USART2_UART_Init — 大彩串口屏 PA2(TX) / PA3(RX)  115200 8N1     */
/*  RX 走 IRQ, 每收到一字节调用 SCREEN_UART_IRQHandler(byte)            */
/* ------------------------------------------------------------------ */
void MX_USART2_UART_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = GPIO_PIN_2;          /* PA2 TX */
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin   = GPIO_PIN_3;          /* PA3 RX */
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;         /* 空闲高, 抗噪 */ 
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        while (1);

    /* 直接使能 RXNE 中断 (比 HAL_UART_Receive_IT 更省事: 不用每次重装) */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* USART2 阻塞发送 —— 串口屏命令一般短, 阻塞即可 */
void UART2_SendByte(uint8_t byte)
{
    HAL_UART_Transmit(&huart2, &byte, 1, HAL_MAX_DELAY);
}

void UART2_SendBytes(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, HAL_MAX_DELAY);
}

/* USART2 中断服务 —— 每收到一字节, 转发给 drv/serial_screen 里的
 * SCREEN_UART_IRQHandler(byte) 状态机. 空闲时是弱空壳, 不影响别的模块. */
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(huart2.Instance->DR & 0xFFU);   /* 读 DR 自动清 RXNE */
        SCREEN_UART_IRQHandler(b);
    }
    /* 其它标志 (ORE/FE/PE) 一并读一下清掉, 避免卡死 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
        (void)huart2.Instance->DR;
    }
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
