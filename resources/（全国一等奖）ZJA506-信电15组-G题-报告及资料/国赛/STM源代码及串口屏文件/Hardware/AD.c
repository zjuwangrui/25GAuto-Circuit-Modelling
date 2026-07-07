#include "AD.h"
#include "stm32f10x.h"
#include "Serial.h"

#define NPT 1024
uint16_t AD_Value_buf[NPT];
uint16_t AD3_Value_buf[NPT];
static DMA_InitTypeDef DMA_InitStructure;
static DMA_InitTypeDef DMA_InitStructure2;

uint8_t adc_flag = 0;
uint8_t adc3_flag = 0;
// DMA中断优先级设置
void ADC_DMA_Init(void){
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1; // 0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void AD_Init(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	ADC_DMA_Init(); // 初始化ADC的DMA中断
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 72/6 = 12MHz
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_ADC1_ETRGREG, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_1Cycles5);
    
    //单通道,非扫描模式,外部触发,数据右对齐
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_Ext_IT11_TIM8_TRGO;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_Init(ADC1, &ADC_InitStructure);
    
    

    //DMA配置
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value_buf;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = NPT;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    //校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET);
    
    // ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
}



void TIM8_init(uint16_t arr,uint16_t psc)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    TIM_InternalClockConfig(TIM8);
    TIM_SelectMasterSlaveMode(TIM8, TIM_MasterSlaveMode_Enable);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_Period = arr - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStructure);
    TIM_SelectOutputTrigger(TIM8, TIM_TRGOSource_Update);
    TIM_Cmd(TIM8, ENABLE);
}

void DMA1_Channel1_IRQHandler(void){
    if(DMA_GetITStatus(DMA1_IT_TC1) != RESET)
	{
        
        adc_flag = 1;
        TIM_Cmd(TIM8, DISABLE);

        // DMA_Cmd(DMA1_Channel1, DISABLE);
		// DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value_buf;
        // DMA_Init(DMA1_Channel1, &DMA_InitStructure);
        // DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
        // DMA_Cmd(DMA1_Channel1, ENABLE);
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    }
}





void AD3_Init(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel4_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1; // 0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 72/6 = 12MHz

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    ADC_RegularChannelConfig(ADC3, ADC_Channel_13, 1, ADC_SampleTime_13Cycles5);

    //单通道,非扫描模式,外部触发,数据右对齐
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T8_TRGO;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_Init(ADC3, &ADC_InitStructure);
   
    

    //DMA配置
    DMA_DeInit(DMA2_Channel5);
    DMA_InitStructure2.DMA_PeripheralBaseAddr = (uint32_t)&ADC3->DR;
    DMA_InitStructure2.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure2.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure2.DMA_MemoryBaseAddr = (uint32_t)AD3_Value_buf;
    DMA_InitStructure2.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure2.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure2.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure2.DMA_BufferSize = 1024;
    DMA_InitStructure2.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure2.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure2.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel5, &DMA_InitStructure2);
    DMA_ITConfig(DMA2_Channel5, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA2_Channel5, ENABLE);

    ADC_DMACmd(ADC3, ENABLE);
    ADC_Cmd(ADC3, ENABLE);

    //校准
    ADC_ResetCalibration(ADC3);
    while(ADC_GetResetCalibrationStatus(ADC3) == SET);
    ADC_StartCalibration(ADC3);
    while(ADC_GetCalibrationStatus(ADC3) == SET);
    
    //ADC_SoftwareStartConvCmd(ADC3,ENABLE);
    ADC_ExternalTrigConvCmd(ADC3, ENABLE);
}
void DMA2_Channel4_5_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_IT_TC5) != RESET)
	{
        
        adc3_flag = 1;
        TIM_Cmd(TIM8, DISABLE);
        
        DMA_ClearITPendingBit(DMA2_IT_TC5);
        
    }
}

void SwitchChannel(uint8_t channel)
{
    if (channel == 1)
    {
        ADC_RegularChannelConfig(ADC1,ADC_Channel_10,1,ADC_SampleTime_13Cycles5);
    }
    else if (channel == 2)
    {
        ADC_RegularChannelConfig(ADC1,ADC_Channel_11,1,ADC_SampleTime_13Cycles5);
    }
    else if (channel == 3)
    {
        ADC_RegularChannelConfig(ADC1,ADC_Channel_12,1,ADC_SampleTime_13Cycles5);
    }

}

void AD_SwitchToEXTI(void) 
{
    // 停止当前ADC和DMA
    ADC_Cmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    
    // 禁用DMA中断
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, DISABLE);
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    
    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_1Cycles5);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_ADC1_ETRGREG, DISABLE);
    // 重新配置ADC为连续转换模式
    ADC_InitTypeDef ADC_InitStructure;
    ADC_StructInit(&ADC_InitStructure);  // 重置结构体
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_Ext_IT11_TIM8_TRGO;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_Init(ADC1, &ADC_InitStructure);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 配置时钟输入引脚 (PB9) 为浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource11);

    EXTI_InitTypeDef EXTI_InitStructure;
    // 配置EXTI9为事件模式（上升沿触发）
    EXTI_InitStructure.EXTI_Line = EXTI_Line11;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // 重新配置DMA
   DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value_buf;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = NPT;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    //校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET);
    
    // ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
}
void AD_SwitchToTIM(void) 
{
    DMA_Cmd(DMA1_Channel1, DISABLE);

    ADC_DMACmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);

    
    // 重新初始化ADC为原始配置
    AD_Init();  // 调用原始初始化函数
    ADC_DMA_Init();
    // 重新启用TIM3触发
    TIM8_init(360,1);
    // 重新启用DMA中断 (关键修复)
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
}
