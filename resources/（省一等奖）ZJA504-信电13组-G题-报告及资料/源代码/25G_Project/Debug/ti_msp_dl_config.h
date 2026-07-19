/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G351X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G351X
#define CONFIG_MSPM0G3519

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA1
#define PWM_0_INST_IRQHandler                                   TIMA1_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA1_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                             32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_0_C0_PORT                                                 GPIOA
#define GPIO_PWM_0_C0_PIN                                         DL_GPIO_PIN_17
#define GPIO_PWM_0_C0_IOMUX                                      (IOMUX_PINCM39)
#define GPIO_PWM_0_C0_IOMUX_FUNC                     IOMUX_PINCM39_PF_TIMA1_CCP0
#define GPIO_PWM_0_C0_IDX                                    DL_TIMER_CC_0_INDEX



/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMG0)
#define TIMER_0_INST_IRQHandler                                 TIMG0_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMG0_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                            (31U)
#define TIMER_0_INST_PUB_0_CH                                                (1)
#define TIMER_0_INST_PUB_1_CH                                                (2)



/* Defines for UART_0 */
#define UART_0_INST                                                        UART1
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART1_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART1_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                         DL_GPIO_PIN_9
#define GPIO_UART_0_TX_PIN                                         DL_GPIO_PIN_8
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM20)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM19)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM20_PF_UART1_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM19_PF_UART1_TX
#define UART_0_BAUD_RATE                                                  (9600)
#define UART_0_IBRD_32_MHZ_9600_BAUD                                       (208)
#define UART_0_FBRD_32_MHZ_9600_BAUD                                        (21)





/* Defines for ADC12_0 */
#define ADC12_0_INST                                                        ADC0
#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_0_ADCMEM_0_REF                DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#define ADC12_0_INST_SUB_CH                                                  (1)
#define GPIO_ADC12_0_C2_PORT                                               GPIOA
#define GPIO_ADC12_0_C2_PIN                                       DL_GPIO_PIN_25

/* Defines for ADC12_1 */
#define ADC12_1_INST                                                        ADC1
#define ADC12_1_INST_IRQHandler                                  ADC1_IRQHandler
#define ADC12_1_INST_INT_IRQN                                    (ADC1_INT_IRQn)
#define ADC12_1_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_1_ADCMEM_0_REF                DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#define ADC12_1_INST_SUB_CH                                                  (2)



/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (1)
#define ADC12_0_INST_DMA_TRIGGER                      (DMA_ADC0_EVT_GEN_BD_TRIG)
/* Defines for DMA_CH1 */
#define DMA_CH1_CHAN_ID                                                      (0)
#define ADC12_1_INST_DMA_TRIGGER                      (DMA_ADC1_EVT_GEN_BD_TRIG)


/* Port definition for Pin Group GPIO_KEY */
#define GPIO_KEY_PORT                                                    (GPIOA)

/* Defines for PIN_A1: GPIOA.1 with pinCMx 2 on package pin 2 */
#define GPIO_KEY_PIN_A1_PIN                                      (DL_GPIO_PIN_1)
#define GPIO_KEY_PIN_A1_IOMUX                                     (IOMUX_PINCM2)
/* Defines for PIN_RESET: GPIOB.2 with pinCMx 15 on package pin 23 */
#define GPIO_AD9910_PIN_RESET_PORT                                       (GPIOB)
#define GPIO_AD9910_PIN_RESET_PIN                                (DL_GPIO_PIN_2)
#define GPIO_AD9910_PIN_RESET_IOMUX                              (IOMUX_PINCM15)
/* Defines for PIN_CS: GPIOA.31 with pinCMx 6 on package pin 7 */
#define GPIO_AD9910_PIN_CS_PORT                                          (GPIOA)
#define GPIO_AD9910_PIN_CS_PIN                                  (DL_GPIO_PIN_31)
#define GPIO_AD9910_PIN_CS_IOMUX                                  (IOMUX_PINCM6)
/* Defines for PIN_SCLK: GPIOA.29 with pinCMx 4 on package pin 4 */
#define GPIO_AD9910_PIN_SCLK_PORT                                        (GPIOA)
#define GPIO_AD9910_PIN_SCLK_PIN                                (DL_GPIO_PIN_29)
#define GPIO_AD9910_PIN_SCLK_IOMUX                                (IOMUX_PINCM4)
/* Defines for PIN_SDIO: GPIOC.17 with pinCMx 70 on package pin 36 */
#define GPIO_AD9910_PIN_SDIO_PORT                                        (GPIOC)
#define GPIO_AD9910_PIN_SDIO_PIN                                (DL_GPIO_PIN_17)
#define GPIO_AD9910_PIN_SDIO_IOMUX                               (IOMUX_PINCM70)
/* Defines for PIN_UPDATE: GPIOC.26 with pinCMx 91 on package pin 90 */
#define GPIO_AD9910_PIN_UPDATE_PORT                                      (GPIOC)
#define GPIO_AD9910_PIN_UPDATE_PIN                              (DL_GPIO_PIN_26)
#define GPIO_AD9910_PIN_UPDATE_IOMUX                             (IOMUX_PINCM91)
/* Defines for PIN_PROFILE0: GPIOC.27 with pinCMx 92 on package pin 91 */
#define GPIO_AD9910_PIN_PROFILE0_PORT                                    (GPIOC)
#define GPIO_AD9910_PIN_PROFILE0_PIN                            (DL_GPIO_PIN_27)
#define GPIO_AD9910_PIN_PROFILE0_IOMUX                           (IOMUX_PINCM92)
/* Defines for PIN_PROFILE1: GPIOC.7 with pinCMx 85 on package pin 79 */
#define GPIO_AD9910_PIN_PROFILE1_PORT                                    (GPIOC)
#define GPIO_AD9910_PIN_PROFILE1_PIN                             (DL_GPIO_PIN_7)
#define GPIO_AD9910_PIN_PROFILE1_IOMUX                           (IOMUX_PINCM85)
/* Defines for PIN_PROFILE2: GPIOB.17 with pinCMx 43 on package pin 73 */
#define GPIO_AD9910_PIN_PROFILE2_PORT                                    (GPIOB)
#define GPIO_AD9910_PIN_PROFILE2_PIN                            (DL_GPIO_PIN_17)
#define GPIO_AD9910_PIN_PROFILE2_IOMUX                           (IOMUX_PINCM43)
/* Defines for PIN_DRCTL: GPIOB.19 with pinCMx 45 on package pin 75 */
#define GPIO_AD9910_PIN_DRCTL_PORT                                       (GPIOB)
#define GPIO_AD9910_PIN_DRCTL_PIN                               (DL_GPIO_PIN_19)
#define GPIO_AD9910_PIN_DRCTL_IOMUX                              (IOMUX_PINCM45)
/* Defines for PIN_DRHOLD: GPIOA.21 with pinCMx 46 on package pin 76 */
#define GPIO_AD9910_PIN_DRHOLD_PORT                                      (GPIOA)
#define GPIO_AD9910_PIN_DRHOLD_PIN                              (DL_GPIO_PIN_21)
#define GPIO_AD9910_PIN_DRHOLD_IOMUX                             (IOMUX_PINCM46)
/* Defines for PIN_PWR: GPIOA.22 with pinCMx 47 on package pin 77 */
#define GPIO_AD9910_PIN_PWR_PORT                                         (GPIOA)
#define GPIO_AD9910_PIN_PWR_PIN                                 (DL_GPIO_PIN_22)
#define GPIO_AD9910_PIN_PWR_IOMUX                                (IOMUX_PINCM47)
/* Defines for PIN_OSK: GPIOB.16 with pinCMx 33 on package pin 50 */
#define GPIO_AD9910_PIN_OSK_PORT                                         (GPIOB)
#define GPIO_AD9910_PIN_OSK_PIN                                 (DL_GPIO_PIN_16)
#define GPIO_AD9910_PIN_OSK_IOMUX                                (IOMUX_PINCM33)



/* Defines for DAC12 */
#define DAC12_IRQHandler                                         DAC0_IRQHandler
#define DAC12_INT_IRQN                                           (DAC0_INT_IRQn)
#define GPIO_DAC12_OUT_PORT                                                GPIOA
#define GPIO_DAC12_OUT_PIN                                        DL_GPIO_PIN_15
#define GPIO_DAC12_IOMUX_OUT                                     (IOMUX_PINCM37)
#define GPIO_DAC12_IOMUX_OUT_FUNC                   IOMUX_PINCM37_PF_UNCONNECTED

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_ADC12_0_init(void);
void SYSCFG_DL_ADC12_1_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_DAC12_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
