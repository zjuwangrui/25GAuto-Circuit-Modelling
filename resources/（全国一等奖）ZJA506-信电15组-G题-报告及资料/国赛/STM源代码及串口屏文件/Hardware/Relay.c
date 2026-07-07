#include "stm32f10x.h"


void Relay_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void Relay_On(void)
{
    GPIO_SetBits(GPIOB,GPIO_Pin_7);
}

void Relay_Off(void)
{
    GPIO_ResetBits(GPIOB,GPIO_Pin_7);
}

