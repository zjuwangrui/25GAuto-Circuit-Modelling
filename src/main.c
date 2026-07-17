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

/* ===== 持续输出三角波任务 =====
 *
 *  走 Mode 2 (RAM 任意波形 playback):
 *    - dds_arb_triangle 会生成 1024 点三角波样点, 灌进 AD9910 内部 RAM,
 *      然后配置 profile 0 为 RAM playback 循环模式
 *    - 播放速率 = SYSCLK/4 / (rate_divider+1) / 1024, 内部换算好
 *    - 1 kHz 三角波: rate_divider ≈ 244, 完全在范围内
 *
 *  每 2 秒重灌一次的用途:
 *    1. 保险 - 万一寄存器被干扰能自动恢复
 *    2. 让 PA4/PA5/PA7/PC4 引脚周期性有 SPI 活动, 方便测线
 *
 *  想切回正弦: 把下面 t_triangle 的 run 字段改回 sine_task_body 即可 (见注释)
 *  想改频率/幅度: 改 dds_arb_triangle 参数
 *  想改 RAM 深度: 改 include/module/dds.h 里 DDS_ARB_RAM_DEPTH (默认 1024)
 */
static void triangle_task(void)
{
    dds_arb_triangle(1000.0 /*Hz*/, 1.0f /*V*/);
    UART_Printf("[tri] refresh uptime=%lu ms  (1kHz 1V triangle, mode=%d)\r\n",
                (unsigned long)HAL_GetTick(), (int)dds_get_mode());
}

/* ===== 持续输出方波任务 =====
 *
 *  同样走 Mode 2 (RAM 任意波形 playback):
 *    - dds_arb_square 会生成 1024 点方波样点 (前 duty*1024 点为 +amp_v,
 *      其余为 -amp_v), 灌进 AD9910 内部 RAM
 *    - duty ∈ (0.01, 0.99), 0.5 = 50% 占空比 (对称方波)
 *
 *  注意: DAC 后面的 300MHz 低通滤波器会让方波的高频谐波损失,
 *        示波器上看到的实际是 "尖角略钝" 的近似方波.
 *        若需要真正的方波, 需在 OUT 后面外接比较器整形.
 *
 *  想改频率/占空比/幅度: 改 dds_arb_square 参数
 */
static void square_task(void)
{
    dds_arb_square(1000.0 /*Hz*/, 0.5f /*duty*/, 1.0f /*V*/);
    UART_Printf("[sqr] refresh uptime=%lu ms  (1kHz 1V 50%% square, mode=%d)\r\n",
                (unsigned long)HAL_GetTick(), (int)dds_get_mode());
}

static void sine_task_body(void) {
    dds_tone_sine(1000.0, 1.0f, 0.0f);
    UART_Printf("[sine] refresh uptime=%lu ms\r\n", (unsigned long)HAL_GetTick());
}


/* ===== 任务表 (在这里增删) ===== */
static sched_task_t t_triangle  = { .run = triangle_task, .period_ms = 2000, .name = "tri" };
static sched_task_t t_square    = { .run = square_task,   .period_ms = 2000, .name = "sqr" };
static sched_task_t t_ui        = { .run = ui_task,       .period_ms = 100,  .name = "ui"  };
static sched_task_t t_dds       = { .run = dds_task,      .period_ms = 10,   .name = "dds" };

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

    /* ---- 上电立即输出波形。三种候选，只留一个不注释：
     *      1) 三角波   dds_arb_triangle(1000.0, 1.0f);
     *      2) 方波     dds_arb_square  (1000.0, 0.5f, 1.0f);
     *      3) 正弦     dds_tone_sine   (1000.0, 1.0f, 0.0f);
     *   任务表同理: 只 sched_register 你想要的那个。 */
    //dds_arb_triangle(1000.0 /*Hz*/, 0.5f /*V*/);//1kHz, 695m偏置,370mVpp,876m,512m. 偏置不变，峰峰值减半。
   dds_arb_square  (1000.0 /*Hz*/, 0.5f /*duty*/, 1.0f /*V*/);//频率500Hz，峰峰值376m，876m,512m，偏置694m

    /* ---- 调度器 ---- */
    sched_init();
   // sched_register(&t_triangle);    /* 持续维持三角波输出 */
    //sched_register(&t_square);      /* 持续维持方波输出   */
    //dds_tone_sine(1000.0, 1.0f, 0.0f);
   // dds_arb_dc(1.0f);
    sched_register(&t_ui);

    sched_run_forever();
}
