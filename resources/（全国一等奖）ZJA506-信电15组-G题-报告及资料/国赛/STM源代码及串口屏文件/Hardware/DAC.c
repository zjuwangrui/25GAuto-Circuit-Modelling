#include "stm32f10x.h"

#define DAC_DHR12RD_ADDRESS      (DAC_BASE+0x20)
void DAC_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DAC_InitTypeDef  DAC_InitStructure;

    /* 使能GPIOA时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* 使能DAC时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    /* DAC的GPIO配置，模拟输入 */
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 配置DAC 通道1 */
    //使用TIM2作为触发源
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_Ext_IT9;
    //不使用波形发生器
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    //不使用DAC输出缓冲
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);

    /*以同样的配置 初始化DAC 通道2 */
    DAC_Init(DAC_Channel_2, &DAC_InitStructure);

    /* 使能通道1 由PA4输出 */
    DAC_Cmd(DAC_Channel_1, ENABLE);
    /* 使能通道2 由PA5输出 */
    DAC_Cmd(DAC_Channel_2, ENABLE);

    /* 使能DAC的DMA请求 */
    DAC_DMACmd(DAC_Channel_2, ENABLE);
}
void DAC_TIM_Config()
{
    TIM_TimeBaseInitTypeDef    TIM_TimeBaseStructure;

    /* 使能TIM2时钟，TIM2CLK 为72M */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* TIM2基本定时器配置 */
    //定时周期 20
    TIM_TimeBaseStructure.TIM_Period = 180-1;
    //预分频，不分频 72M / (0+1) = 72M
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0;
    //时钟分频系数
    TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
    //向上计数模式
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* 配置TIM2触发源 */
    TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);

    /* 使能TIM2 */
    TIM_Cmd(TIM2, ENABLE);
}
void DAC_EXTI_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 配置时钟输入引脚 (PB9) 为浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource9);

    EXTI_InitTypeDef EXTI_InitStructure;
    // 配置EXTI9为事件模式（上升沿触发）
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
}

//正弦波单个周期的点数
#define POINT_NUM 1024
/* 波形数据 -----------------------------------------------*/

uint32_t DualSine12bit[POINT_NUM];



/**
* @brief  配置DMA
* @param  无
* @retval 无
*/
void DAC_DMA_Config(void)
{
    DMA_InitTypeDef  DMA_InitStructure;

    /* 使能DMA2时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    /* 配置DMA2 */
    //外设数据地址
    DMA_InitStructure.DMA_PeripheralBaseAddr = DAC_DHR12RD_ADDRESS;
    //内存数据地址 DualSine12bit
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&DualSine12bit ;
    //数据传输方向内存至外设
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    //缓存大小为POINT_NUM字节
    DMA_InitStructure.DMA_BufferSize = POINT_NUM;
    //外设数据地址固定
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    //内存数据地址自增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    //外设数据以字为单位
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    //内存数据以字为单位
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    //循环模式
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    //高DMA通道优先级
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    //非内存至内存模式
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

    DMA_Init(DMA2_Channel4, &DMA_InitStructure);

    /* 使能DMA2-14通道 */
    DMA_Cmd(DMA2_Channel4, ENABLE);
}
/**
* @brief  DAC初始化函数
* @param  无
* @retval 无
*/
 uint32_t Idx = 0;
void DAC_Mode_Init(uint16_t * data)
{
   

    DAC_Config();
    DAC_EXTI_Init();

    /* 填充正弦波形数据，双通道右对齐*/
    for (Idx = 0; Idx < POINT_NUM; Idx++) {
    DualSine12bit[Idx] = (data[Idx] << 16) + (data[Idx]);
    }
    DAC_DMA_Config();
}

