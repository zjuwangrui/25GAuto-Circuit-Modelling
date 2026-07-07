#include "stm32f1xx_hal.h"
#include "uart.h"

/* ------------------------------------------------------------------ */
/*  LED — PB0, active low                                              */
/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/*  System clock — HSI/2 x16 = 64 MHz                                 */
/* ------------------------------------------------------------------ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;   /* 4 MHz */
    osc.PLL.PLLMUL          = RCC_PLL_MUL16;            /* 64 MHz */
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
    UART_Init();

    /* Startup banner */
    UART_SendString("\r\n");
    UART_SendString("============================================\r\n");
    UART_SendString("  VTL Sensor System  —  UART Test\r\n");
    UART_SendString("  Board : Wildfire Guide  (STM32F103VE)\r\n");
    UART_SendString("  USART1: PA9(TX) / PA10(RX)  115200 8N1\r\n");
    UART_SendString("============================================\r\n");
    UART_Printf("  SysClock : %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    UART_Printf("  PCLK1    : %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    UART_Printf("  PCLK2    : %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
    UART_SendString("--------------------------------------------\r\n");
    UART_SendString("  Heartbeat every 2 s  |  echo on receive\r\n");
    UART_SendString("============================================\r\n\r\n");

    uint32_t hb_count  = 0;
    uint32_t last_tick = HAL_GetTick();

    /* Line echo buffer */
    uint8_t  line_buf[128];
    uint16_t line_len = 0;

    while (1) {
        /* Heartbeat */
        if (HAL_GetTick() - last_tick >= 2000) {
            last_tick = HAL_GetTick();
            UART_Printf("[%8lu ms] Heartbeat #%lu\r\n", last_tick, ++hb_count);
        }

        /* Echo — non-blocking receive (10 ms timeout) */
        uint8_t rx = UART_ReceiveByte(10);
        if (rx == 0)
            continue;

        /* Immediate single-byte echo so the terminal shows what you type */
        UART_SendByte(rx);

        if (rx == '\r' || rx == '\n') {
            if (line_len == 0) {
                UART_SendString("\r\n");
                continue;
            }

            /* Line echo: print received string */
            UART_SendString("\r\n");
            UART_Printf("[ECHO] %.*s  (%u bytes)\r\n", (int)line_len, line_buf, line_len);

            /* Hex dump of the received line */
            UART_SendString("[HEX ] ");
            for (uint16_t i = 0; i < line_len; i++)
                UART_Printf("%02X ", line_buf[i]);
            UART_SendString("\r\n\r\n");

            line_len = 0;
        } else if (rx == 0x08 || rx == 0x7F) {
            /* Backspace: erase last character */
            if (line_len > 0) {
                line_len--;
                UART_SendString("\b \b");   /* erase char on terminal */
            }
        } else {
            /* Normal character: buffer it */
            if (line_len < sizeof(line_buf) - 1)
                line_buf[line_len++] = rx;
        }
    }
}

void Error_Handler(void) { __disable_irq(); while (1); }
