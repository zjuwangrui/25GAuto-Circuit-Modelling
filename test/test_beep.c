#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "stm32f10x.h" 
// 蜂鸣器引脚定义
#define BEEP_PIN                    GPIO_PIN_8      // PA8引脚
#define BEEP_PORT                   GPIOA           // GPIOA端口
#define BEEP_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()  // 使能GPIOA时钟

// 蜂鸣器状态定义
#define BEEP_OFF                     GPIO_PIN_RESET  // 低电平关闭（根据实际电路修改）
#define BEEP_ON                    GPIO_PIN_SET    // 高电平开启
// 注意：如果蜂鸣器是高电平有效，请交换上面两个宏定义的值

// 函数声明
static void Beep_Init(void);        // 蜂鸣器初始化
static void Beep_On(void);          // 蜂鸣器开启
static void Beep_Off(void);         // 蜂鸣器关闭
static void SystemClock_Config(void); // 系统时钟配置
static void led_on(void);

int main(void)
{
    // 初始化HAL库
    HAL_Init();
    
    // 配置系统时钟为72MHz
    SystemClock_Config();
    
    // 初始化蜂鸣器引脚
    Beep_Init();
    
    // 主循环 - 实现高低电平反复切换
    while (1)
    {
        // 方法1：使用延时实现固定频率鸣叫（1秒周期）
        Beep_On();              // 蜂鸣器开启（高电平）
        HAL_Delay(500);         // 持续500毫秒
        Beep_Off();             // 蜂鸣器关闭（低电平）
        HAL_Delay(500);         // 停止500毫秒
		led_on();
    }
}

/**
 * @brief 蜂鸣器初始化函数
 * @param 无
 * @retval 无
 */
static void Beep_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能GPIOA时钟
    BEEP_CLK_ENABLE();
    
    // 配置PA8引脚为推挽输出模式
    GPIO_InitStruct.Pin = BEEP_PIN;                 // PA8引脚
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;     // 推挽输出模式
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // 高速输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;             // 无上下拉电阻
    HAL_GPIO_Init(BEEP_PORT, &GPIO_InitStruct);
    
    // 初始化关闭蜂鸣器（输出低电平）
    HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, BEEP_OFF);
}

/**
 * @brief 开启蜂鸣器
 * @param 无
 * @retval 无
 */
static void Beep_On(void)
{
    HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, BEEP_ON);
}

/**
 * @brief 关闭蜂鸣器
 * @param 无
 * @retval 无
 */
static void Beep_Off(void)
{
    HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, BEEP_OFF);
}


// /**
//  * @brief 系统时钟配置函数
//  * @note 配置系统时钟为72MHz
//  * @param 无
//  * @retval 无
//  */
// static void SystemClock_Config(void)
// {
//     RCC_OscInitTypeDef RCC_OscInitStruct = {0};
//     RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
//     // 配置外部晶振（HSE）8MHz
//     RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//     RCC_OscInitStruct.HSEState = RCC_HSE_ON;              // 开启HSE
//     RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
//     RCC_OscInitStruct.HSIState = RCC_HSI_ON;
//     RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;          // 开启PLL
//     RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;  // PLL时钟源为HSE
//     RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;          // 8MHz * 9 = 72MHz
//     HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
//     // 配置系统时钟
//     RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
//                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
//     RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 系统时钟选择PLL
//     RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // HCLK = SYSCLK = 72MHz
//     RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;          // APB1 = HCLK/2 = 36MHz
//     RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;          // APB2 = HCLK = 72MHz
//     HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);   // Flash延迟2个周期
// }


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 72000000
  *            HCLK(Hz)                       = 72000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            HSE PREDIV1                    = 1
  *            PLLMUL                         = 9
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
  oscinitstruct.HSEState        = RCC_HSE_ON;
  oscinitstruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }
}

// 错误处理函数（HAL库错误时调用）
void Error_Handler(void)
{
    while(1)
    {
        // 错误时蜂鸣器快速鸣叫
        HAL_GPIO_TogglePin(BEEP_PORT, BEEP_PIN);
        HAL_Delay(100);
    }
}
void led_on(void)
{
	RCC_APB2ENR |= (1 << 3);
	GPIOB_CRL &= ~(0x0F << (4 * 0));
	GPIOB_CRL |= (1 << 4 * 0);
	GPIOB_ODR &= (1 << 0);
}
