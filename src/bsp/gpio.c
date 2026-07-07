#include "bsp/gpio.h"

/*
 * 框架层不假设具体引脚。各模块（led / uart / adc / esp8266 …）
 * 在自己 init 里配置自己的 GPIO
 *
 * 这里只做一件事：在启动早期统一使能所有 GPIO 时钟，之后各模块
 * 就不必重复 __HAL_RCC_GPIOx_CLK_ENABLE()。
 */
void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
}
