#include "stm32f10x.h"                  // Device header

#define BULE_PIN	GPIO_Pin_1
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Pin = BULE_PIN;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	
	GPIO_SetBits(GPIOB, BULE_PIN);
}
void LED_Off(void)
{
	GPIO_SetBits(GPIOB, BULE_PIN);
}

void LED_On(void)
{
	
	GPIO_ResetBits(GPIOB, BULE_PIN);
}
