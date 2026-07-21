/**
 * main.c —— STM32 App Framework 骨架
 *
 * 只做四件事：
 *   1) 内核: HAL_Init + 系统时钟 (HSE 8M × 9 = 72MHz)
 *   2) BSP : GPIO / USART1 (调试) / ADC
 *   3) 通用模块 & 驱动的 init
 *   4) 注册调度任务 → sched_run_forever
 *
 * 增加业务模块的步骤:
 *   - include/module/xxx.h + src/module/xxx.c: void xxx_init/void xxx_task
 *   - 若需要新驱动: include/drv/yyy.h + src/drv/yyy.c
 *   - 在下方 "任务表" 里加一行 sched_task_t，并在 main() 里 sched_register
 *
 * 任务耗时约束 (详见 core/scheduler.h):
 *   - 禁用 HAL_Delay / 阻塞轮询
 *   - 单次执行时间 < 5ms
 *   - 长操作用状态机分帧
 */

#include "stm32f1xx_hal.h"

#include "bsp/gpio.h"
#include "bsp/uart.h"
#include "bsp/adc.h"
#include "bsp/led.h"

#include "core/scheduler.h"
#include "module/ui.h"
#include "module/thd.h"
#include "module/dds.h"
#include "module/signal_out.h"
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
        osc.HSIState            = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
        osc.PLL.PLLMUL          = RCC_PLL_MUL16;
        if (HAL_RCC_OscConfig(&osc) != HAL_OK) while (1);
    }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) while (1);
}




/* ===== 任务表 (在这里增删) ===== */
static sched_task_t t_ui        = { .run = ui_task,       .period_ms = 100,  .name = "ui"  };
static sched_task_t t_signal_out = { .run = signal_out_task, .period_ms = 3000, .name = "sigout" };
int main(void)
{
    /* ---- 内核 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- BSP ---- */
    MX_GPIO_Init();
    MX_USART1_UART_Init();          /* 调试口 (UART_Printf 走这里) */
    MX_USART2_UART_Init();          /* 大彩串口屏 PA2/PA3 @115200 */

    /* ---- 通用模块 & 驱动 ---- */
    ui_init();
    dds_init();                     /* dds_init 内部会 ad9910_init: SPI/复位/PLL 锁定 */

    /* ---- 调度器 ---- */
    sched_init();
    signal_out_set(100.0, 1.5f);       
   //dds_tone_sine(1000.0, 1.0f, 0.0f);          /* 上电即输出 1kHz/1Vpp 正弦 */
    sched_register(&t_signal_out);//打印信息
    //dds_tone_sine(1000.0, 1.0f, 0.0f);          /* 上电即输出 1kHz/0.5Vpp 正弦 */
    sched_run_forever();
}
