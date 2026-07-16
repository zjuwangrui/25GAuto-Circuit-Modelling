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

/* ===== 持续输出正弦任务 =====
 *
 *  每 2 秒重发一次 profile 0 参数 (1kHz / 1V / 0°)：
 *    - AD9910 profile 0 参数其实是"写一次就一直有效"的，本来无需重发
 *    - 每 2s 刷一次的实际用途：
 *        1. 万一芯片被外部干扰丢了寄存器，能自动恢复
 *        2. 示波器上能看到周期性 SPI 活动 (PA4/PA5/PA7 + PC4)，方便测线
 *    - 输出永远保持 1kHz 正弦，不做静音也不做复位
 *
 *  想改频率/幅度：改下面的 dds_tone_sine 参数
 *  想加快/减慢刷新：改 t_sine.period_ms
 */
static void sine_task(void)
{
    dds_tone_sine(1000.0 /*Hz*/, 1.0f /*V*/, 0.0f /*deg*/);
    UART_Printf("[sine] refresh uptime=%lu ms  (1kHz 1V, mode=%d)\r\n",
                (unsigned long)HAL_GetTick(), (int)dds_get_mode());
}

/* ===== 任务表 (在这里增删) ===== */
static sched_task_t t_sine      = { .run = sine_task, .period_ms = 2000, .name = "sine" };
static sched_task_t t_ui        = { .run = ui_task,   .period_ms = 100,  .name = "ui"   };
static sched_task_t t_dds       = { .run = dds_task,  .period_ms = 10,   .name = "dds"  };

int main(void)
{
    /* ---- 内核 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- BSP ---- */
    MX_GPIO_Init();
    MX_USART1_UART_Init();          /* 调试口 (UART_Printf 走这里) */

    /* ---- 通用模块 & 驱动 ---- */
    ui_init();
    dds_init();                     /* dds_init 内部会 ad9910_init: SPI/复位/PLL 锁定 */

    /* ---- 上电立即输出一次 1kHz 正弦，1V 幅度 ---- */
    dds_tone_sine(1000.0 /*Hz*/, 1.0f /*V*/, 0.0f /*deg*/);

    /* ---- 调度器 ---- */
    sched_init();
    sched_register(&t_sine);        /* 持续维持正弦输出 */
    sched_register(&t_ui);
    //sched_register(&t_dds);         /* dds_task: 空跑; 进入 HOP 自动模式后才干活 */

    sched_run_forever();
}
