/**
 * main.c — STM32 App Framework 骨架
 *
 * 只做：时钟 → BSP 初始化 → 注册核心任务 → 交给调度器。
 *
 * 增加业务模块（比如 THD 测量、按键、通信）的步骤：
 *   1) 在 include/module/xxx.h 与 src/module/xxx.c 里写：
 *        void xxx_init(void);
 *        void xxx_task(void);
 *   2) 需要新驱动就写 include/drv/yyy.h 与 src/drv/yyy.c，
 *      在 xxx_init 里调用其初始化。
 *   3) 在下方任务表里加两行：初始化 + sched_register。
 *
 * 各模块耗时约束（详见 core/scheduler.h）：
 *   - 任务函数禁用 HAL_Delay
 *   - 单次执行时间 < 5ms
 *   - 长操作请状态机化
 */

#include "stm32f1xx_hal.h"

#include "bsp/gpio.h"
#include "bsp/led.h"
#include "bsp/uart.h"
#include "bsp/adc.h"

#include "core/scheduler.h"
#include "module/terminal.h"
#include "module/ui.h"
#include "module/fft.h"
#include "module/thd.h"
#include "drv/ad9910.h"

/* ===== 系统时钟：HSE 8MHz × PLL×9 = 72MHz，HSE 失败回退 HSI ===== */
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

/* ===== 心跳任务：红灯 500ms 翻转，作为"程序在跑"的可视化指示 ===== */
static void heartbeat_task(void) { LED_Toggle(LED_RED); }

/* ===== 任务表（在这里增删） ===== */
static sched_task_t t_heartbeat = { .run = heartbeat_task, .period_ms = 500, .name = "heart" };
static sched_task_t t_terminal  = { .run = terminal_task,  .period_ms = 10,  .name = "term"  };
static sched_task_t t_ui        = { .run = ui_task,        .period_ms = 100, .name = "ui"    };
//static sched_task_t t_fft       = { .run = fft_task,       .period_ms = 0,   .name = "fft"   };
static sched_task_t t_thd       = { .run = thd_task,       .period_ms = 0,   .name = "thd"   };

int main(void)
{
    /* ---- 内核 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- BSP ---- */
    MX_GPIO_Init();                /* 使能 GPIO 时钟 */
    MX_USART1_UART_Init();         /* 调试口 + terminal 输入 */
    MX_ADC1_Init();

    /* ---- 通用模块 ---- */
    terminal_init();
    ui_init();
    thd_init();
    ad9910_init();                 /* AD9910 上电 + PLL 锁定 + 默认 1kHz 输出 */

    thd_configure(ADC_CHANNEL_1, 1000000);  /* THD 测量：PA1，采样率 1MHz */
    thd_start();
    ui_switch_to("thd");
    /* ---- 调度器 ---- */
    sched_init();
    sched_register(&t_heartbeat);
    sched_register(&t_terminal);
    sched_register(&t_ui);
    //sched_register(&t_fft);
    sched_register(&t_thd);


    sched_run_forever();           /* 不返回 */
}
