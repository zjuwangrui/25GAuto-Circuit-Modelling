
#include "stm32f1xx_hal.h"
/* ------------------------------------------------------------------ */
/*  Buzzer — PA8, active high                                          */
/* ------------------------------------------------------------------ */
static void LED_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_0;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void LED_On(void)  { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);   }
static void LED_Off(void) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); }

/* ------------------------------------------------------------------ */
/*  System clock — HSI 8 MHz, PLL x9 = 64 MHz (HSI/2 x16 不可达72)  */
/*  HSI 8MHz -> /2 -> PLL x16 = 64 MHz                               */
/* ------------------------------------------------------------------ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI_DIV2;  /* HSI/2 = 4 MHz */
    osc.PLL.PLLMUL     = RCC_PLL_MUL16;           /* 4 x16 = 64 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        while (1);

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        while (1);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    while (1) {
        LED_On();
        HAL_Delay(1000);          /* 停 1 秒 */
        LED_Off();   /* 长鸣 1 声 */
        HAL_Delay(1000);            /* 停 1 秒 */
    }
}

void Error_Handler(void) { while (1); }
