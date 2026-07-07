#ifndef __AD_H
#define __AD_H

#include "stm32f10x.h"
#include "stdint.h"

#define NPT 1024



void AD_Init(void);
void AD3_Init(void);
void TIM8_init(uint16_t arr,uint16_t psc);
void ADC_DMA_Init(void);
void SwitchChannel(uint8_t channel);
void AD_SwitchToEXTI(void);
void AD_SwitchToTIM(void) ;
extern uint8_t adc_flag; 
extern uint8_t adc3_flag;
extern uint16_t AD_Value_buf[NPT];
extern uint16_t AD3_Value_buf[NPT];
#endif




