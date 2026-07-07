/**********************************************************
                       ��������
������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;											 
���ܣ�DAC902ģ��-�����ѹ
ʱ�䣺2024/2024/12/5
�汾��1.4
���ߣ���������
�������������������֤���������ȷ����������ӹ��������б�д����
			������������뵽�Ա��꣬�������ӽ߳�Ϊ������ ^_^
			https://kvdz.taobao.com/
**********************************************************/
# include "DAC902.h"
#include "timer.h"
#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "math.h"

#define SIN_POINTS 32 // 正弦波一个周期的点数
uint16_t sin_table[SIN_POINTS]; // 正弦波数据表
uint16_t sin_index = 0;   


typedef struct {
    GPIO_TypeDef* port;  // GPIO�˿�
    uint16_t pin;        // ���ź�
} DataPin;

const DataPin dataPins[12] = {
    {GPIOC, GPIO_Pin_8},   
    {GPIOD, GPIO_Pin_2},   
    {GPIOC, GPIO_Pin_9},  
    {GPIOC, GPIO_Pin_12},
	{GPIOC, GPIO_Pin_6},
	{GPIOC, GPIO_Pin_7},
	{GPIOB, GPIO_Pin_7},
	{GPIOB, GPIO_Pin_9},
	{GPIOA, GPIO_Pin_8},
	{GPIOA, GPIO_Pin_6},
	{GPIOE, GPIO_Pin_5},
	{GPIOC, GPIO_Pin_13},
};
void Generate_Sin_Table(void) {
    for (int i = 0; i < SIN_POINTS; i++) {
        // 生成[0, 2π]区间内的正弦值，并映射到0-4095范围
        float radian = 2 * 3.1415926f * i / SIN_POINTS;
        float sin_val = sinf(radian);                // [-1, 1]
        sin_table[i] = (uint16_t)(2048 * sin_val + 2048); // [0, 4095]
    }
}

u32 Fre_In = 20000;//����Ƶ��	1K Hz
uint8_t duty = 50; //ռ�ձ��޸ı���

uint16_t down_point = 64;//�½���
uint16_t down_add = 4096;//��������
uint16_t up_point =  0;//������
uint16_t up_add = 0;//��������
uint16_t all_point=200;//һ�������ڵ��ܵ���
uint16_t down_dip=0,up_dip=0;//���� ����

uint16_t data = 0;//����
uint8_t down = 0;//����

/**************************************************************** 
** �������� ��DAC902_IO_Init
** �������� ����ʼ�� DAC902 I/O������
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
*****************************************************************/ 
void DAC902_IO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStructure ; 

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE, ENABLE);	 //ʹ��PA,PC�˿�ʱ��

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6| GPIO_Pin_7| GPIO_Pin_8| GPIO_Pin_9| GPIO_Pin_12| GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOC ,&GPIO_InitStructure) ;

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2; 
	GPIO_Init(GPIOD ,&GPIO_InitStructure) ;
	
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5; 
	GPIO_Init(GPIOE ,&GPIO_InitStructure) ;
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4| GPIO_Pin_6 | GPIO_Pin_8 | GPIO_Pin_12;
	GPIO_Init(GPIOA ,&GPIO_InitStructure) ;
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7| GPIO_Pin_9;
	GPIO_Init(GPIOB ,&GPIO_InitStructure) ;

}
/**************************************************************** 
** �������� ��DAC902_WriteData
** �������� ��д����
** ��ڲ��� ��dat--���� ��Χ(0~4095)
** ���ڲ��� ����
** ����˵�� ����
*****************************************************************/
void DAC902_WriteData(uint16_t dat)
{
	uint8_t i;
	for(i = 0; i < 12; i++) {
        if(dat & (1 << i)) {
            GPIO_SetBits(dataPins[i].port, dataPins[i].pin);
        } else {
            GPIO_ResetBits(dataPins[i].port, dataPins[i].pin);
        }
    }
	
	DAC902_CLK = 1;
	DAC902_CLK = 0;

}
/**************************************************************** 
** �������� ��DAC902_WriteData
** �������� ��д����
** ��ڲ��� ��mfre--Ƶ��
** ���ڲ��� ����
** ����˵�� ����
*****************************************************************/
void Set_Fre(u32 mfre) {
    TIM_Cmd(TIM4, DISABLE); // 暂停定时器
    
    if (mfre == 0) mfre = 1; // 防止除零错误
    
    // 计算定时器重装载值 = 时钟频率 / (波形频率 * 点数)
    u32 timer_clock = 72000000; // TIM4时钟72MHz
    u32 arr_val = timer_clock / (mfre * SIN_POINTS) - 1;
    
    TIM_SetAutoreload(TIM4, arr_val); // 设置自动重装载值
    TIM_Cmd(TIM4, ENABLE);  // 重启定时器
}
/**************************************************************** 
** �������� ��Set_Fre
** �������� ����ʼ������
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
*****************************************************************/
void Set_UpDown_Point(void)
{
		Set_Fre(Fre_In);
		down_dip = 0;
		up_dip = 0;
		up_point = 0;
		down_point = 0;
		data = 0;
		down = 0;
		down_point=all_point*duty/100;//�����½��׶εĵ���
		up_point = all_point - down_point;//���������׶εĵ���
		down_add = 4096/down_point;//DACֵ�仯����
		up_add = down_add*down_point/up_point;//ÿ�������Ĳ���
}

/**************************************************************** 
** �������� ��DAC902_Init
** �������� ����ʼ��DAC902ģ��
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
*****************************************************************/
void DAC902_Init(void)
{
	DAC902_IO_Init();
	Generate_Sin_Table();
	sin_index = 0;        // 重置索引
	Set_UpDown_Point();
	DAC902_CLK = 0;
	DAC902_PowerON();
	DAC902_WriteData(4094);
	DAC902_WriteData(4094);
}
/**************************************************************** 
** �������� ��Triangle_Wave
** �������� ���������ǲ�����
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ���ڶ�ʱ���жϵ��ô˺���
*****************************************************************/
void Triangle_Wave(void)
{
	//�޷�
	if(data >= 0X0FFF)
		data = 0X0FFF;
	if(!down)
	{
		DAC902_WriteData(data);
		down_dip++;
		if(down_dip >= down_point)
		{
			data -=up_add;
			down = 1;
			up_dip=0;
		}
		else 
			data +=down_add;
	}
	else
	{
			DAC902_WriteData(data);
			up_dip++;
			if(up_dip >= up_point)
			{
				
				data = down_add;
				down = 0;
				down_dip=0;
			}
			else 	
				data -=up_add;
	}
}


void Sine_Wave(void) {
    DAC902_WriteData(sin_table[sin_index]); // 输出当前正弦波点
    
    sin_index++; // 移动到下一个点
    if (sin_index >= SIN_POINTS) {
        sin_index = 0; // 完成一个周期后重置
    }
}
