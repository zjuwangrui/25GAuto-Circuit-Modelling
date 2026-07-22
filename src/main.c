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
#include "module/panel_ctrl.h"
#include "module/learning.h"
#include "module/inference.h"
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
static sched_task_t t_ui         = { .run = ui_task,          .period_ms = 100,  .name = "ui"  };
static sched_task_t t_signal_out = { .run = signal_out_task,  .period_ms = 3000, .name = "sigout" };
/* 屏任务: 每 50 ms 处理一次 UART2 收到的屏事件.
 *   - drv/serial_screen 在 UART2 中断里 (SCREEN_UART_IRQHandler) 组好完整帧,
 *     调 panel_ctrl.on_frame (仍在中断里) 存待办 flag / 缓存 freq/vpp.
 *   - panel_ctrl_task 在这里的 task 上下文里检查 flag, 真正调 signal_out_set. */
static sched_task_t t_panel      = { .run = panel_ctrl_task,  .period_ms = 50,   .name = "panel" };
/* 学习任务: 10 ms tick 推进扫频状态机 (M3, 见 module/learning) */
static sched_task_t t_learning   = { .run = learning_task,    .period_ms = 10,   .name = "learn" };
/* 推理任务: IIR 全部在 ADC 回调里跑, 这里只做偶发检查 (M4) */
static sched_task_t t_inference  = { .run = inference_task,   .period_ms = 100,  .name = "infer" };

int main(void)
{
    /* ---- 内核 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- BSP ---- */
    MX_GPIO_Init();
    MX_USART1_UART_Init();          /* 调试口 (UART_Printf 走这里) */
    MX_USART2_UART_Init();          /* 大彩串口屏 PA2/PA3 @115200 */
    MX_ADC1_Init();                 /* 学习/推理都要用 */

    /* ---- 通用模块 & 驱动 ---- */
    ui_init();
    dds_init();                     /* dds_init 内部会 ad9910_init: SPI/复位/PLL 锁定 */
    panel_ctrl_init();              /* 注册屏事件回调 (内部再次 MX_USART2_UART_Init 无害) */
    learning_init();                /* 学习状态机 + 频率表初始化 */
    inference_init();               /* 推理 biquad + DAC 初始化 */

    /* ---- 调度器 ---- */
    sched_init();
    //sched_register(&t_ui);
    //sched_register(&t_panel);       /* 屏事件驱动: 输入 + 输出 + 学习 + 推理按钮 */
    //sched_register(&t_learning);    /* 学习状态机 10 ms tick */
    //sched_register(&t_inference);   /* 推理观察任务 (真活在 ADC 回调) */
    //sched_register(&t_signal_out);
    dds_tone_sine(3000,0.1f,0.0f);
    sched_run_forever();
}
