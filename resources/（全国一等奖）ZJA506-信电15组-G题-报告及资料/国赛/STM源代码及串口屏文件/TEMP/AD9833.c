#include "AD9833.h"
#include "Delay.h"

//ʱ������Ϊ25 MHzʱ�� ����ʵ��0.1 Hz�ķֱ��ʣ���ʱ������Ϊ1 MHzʱ�������ʵ��0.004 Hz�ķֱ��ʡ�
//�����ο�ʱ���޸Ĵ˴����ɡ�
#define FCLK 25000000	//���òο�ʱ��25MHz����Ĭ�ϰ��ؾ���Ƶ��25Mhz��

#define RealFreDat    268435456.0/FCLK//�ܵĹ�ʽΪ Fout=��Fclk/2��28�η���*28λ�Ĵ�����ֵ

void AD9833_GPIO_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(AD9833_GPIO_RCC,ENABLE);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz; 
	GPIO_InitStructure.GPIO_Pin=AD9833_1_GPIO_D|AD9833_GPIO_CLK
	|AD9833_GPIO_SYNC|AD9833_2_GPIO_D;
	
	GPIO_Init(AD9833_GPIO_PORT,&GPIO_InitStructure);
}

unsigned char AD9833_SPI_Write(unsigned char* data1,unsigned char* data2,unsigned char bytesNumber)
{
	unsigned char i,j; 
	unsigned char writeData1[5]	= {0,0, 0, 0, 0};
	unsigned char writeData2[5]	= {0,0, 0, 0, 0};
	
	ad9833_CLK_h; 
	ad9833_SYNC_l; 

	for(i = 0;i < bytesNumber;i ++)
	{
		writeData1[i] = data1[i + 1];
		writeData2[i] = data2[i + 1];
	}

	for(i=0 ;i<bytesNumber ;i++) 
	{
		for(j=0 ;j<8 ;j++)      
		{ 
			if(writeData1[i] & 0x80)
			{
				ad9833_1_D_h;
			}
			else
			{
				ad9833_1_D_l; 
			}
			
			if(writeData2[i] & 0x80)
			{
				ad9833_2_D_h;
			}
			else
			{
				ad9833_2_D_l; 
			}
			
			ad9833_CLK_l; 
			writeData1[i] <<= 1; 
			writeData2[i] <<= 1; 
			ad9833_CLK_h; 
		} 
	}
	
	ad9833_1_D_h; 
	ad9833_2_D_h;
	ad9833_SYNC_h; 

	return i;
}

/************************************************************
** �������� ��void AD9833_Init(void)  
** �������� ����ʼ������AD9833��Ҫ�õ���IO�ڼ��Ĵ���
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
**************************************************************/
void AD9833_Init(void)
{
    AD9833_GPIO_Init();
    AD9833_SetRegisterValue(AD9833_REG_CMD | AD9833_RESET,AD9833_REG_CMD | AD9833_RESET);
}

/*****************************************************************************************
** �������� ��void AD9833_Reset(void)  
** �������� ������AD9833�ĸ�λλ
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_Reset(void)
{
	AD9833_SetRegisterValue(AD9833_REG_CMD | AD9833_RESET,AD9833_REG_CMD | AD9833_RESET);
	Delay_ms(10);
}

/*****************************************************************************************
** �������� ��void AD9833_ClearReset(void)  
** �������� �����AD9833�ĸ�λλ��
** ��ڲ��� ����
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_ClearReset(void)
{
	AD9833_SetRegisterValue(AD9833_REG_CMD,AD9833_REG_CMD);
}

/*****************************************************************************************
** �������� ��void AD9833_SetRegisterValue(unsigned short regValue)
** �������� ����ֵд��Ĵ���
** ��ڲ��� ��regValue��Ҫд��Ĵ�����ֵ��
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_SetRegisterValue(unsigned short regValue1,unsigned short regValue2)
{
	unsigned char data1[5] = {0x03, 0x00, 0x00};	
	unsigned char data2[5] = {0x03, 0x00, 0x00};
	
	data1[1] = (unsigned char)((regValue1 & 0xFF00) >> 8);
	data1[2] = (unsigned char)((regValue1 & 0x00FF) >> 0);
	
	data2[1] = (unsigned char)((regValue2 & 0xFF00) >> 8);
	data2[2] = (unsigned char)((regValue2 & 0x00FF) >> 0);
	
	AD9833_SPI_Write(data1,data2,2);
}

/*****************************************************************************************
** �������� ��void AD9833_SetFrequencyQuick(float fout,unsigned short type)
** �������� ��д��Ƶ�ʼĴ���
** ��ڲ��� ��val��Ҫд���Ƶ��ֵ��
**						type���������ͣ�AD9833_OUT_SINUS���Ҳ���AD9833_OUT_TRIANGLE���ǲ���AD9833_OUT_MSB����
** ���ڲ��� ����
** ����˵�� ��ʱ������Ϊ25 MHzʱ�� ����ʵ��0.1 Hz�ķֱ��ʣ���ʱ������Ϊ1 MHzʱ�������ʵ��0.004 Hz�ķֱ��ʡ�
*******************************************************************************************/
void AD9833_SetFrequencyQuick(float fout1,unsigned short type1,float fout2,unsigned short type2)
{
	AD9833_SetFrequency(AD9833_REG_FREQ0,fout1,type1,fout2,type2);
}

/*****************************************************************************************
** �������� ��void AD9833_SetFrequency(unsigned short reg, float fout,unsigned short type)
** �������� ��д��Ƶ�ʼĴ���
** ��ڲ��� ��reg��Ҫд���Ƶ�ʼĴ�����
**						val��Ҫд���ֵ��
**						type���������ͣ�AD9833_OUT_SINUS���Ҳ���AD9833_OUT_TRIANGLE���ǲ���AD9833_OUT_MSB����
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_SetFrequency(unsigned short reg, float fout1,unsigned short type1,float fout2,unsigned short type2)
{
	unsigned short freqHi1 = reg;
	unsigned short freqLo1 = reg;
	unsigned long val1=RealFreDat*fout1;
	
	unsigned short freqHi2 = reg;
	unsigned short freqLo2 = reg;
	unsigned long val2=RealFreDat*fout2;
	
	freqHi1 |= (val1 & 0xFFFC000) >> 14 ;
	freqLo1 |= (val1 & 0x3FFF);
	
	freqHi2 |= (val2 & 0xFFFC000) >> 14 ;
	freqLo2 |= (val2 & 0x3FFF);
	
	AD9833_SetRegisterValue(AD9833_B28|type1,AD9833_B28|type2);
	AD9833_SetRegisterValue(freqLo1,freqLo2);
	AD9833_SetRegisterValue(freqHi1,freqHi2);
}

/*****************************************************************************************
** �������� ��void AD9833_SetPhase(unsigned short reg, unsigned short val)
** �������� ��д����λ�Ĵ�����
** ��ڲ��� ��reg��Ҫд�����λ�Ĵ�����
**						val��Ҫд���ֵ��
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_SetPhase(unsigned short reg, unsigned short val1, unsigned short val2)
{
	unsigned short phase1 = reg,phase2 = reg;
	phase1 |= val1;
	phase2 |= val2;
	AD9833_SetRegisterValue(phase1,phase2);
}

/*****************************************************************************************
** �������� ��void AD9833_Setup(unsigned short freq, unsigned short phase,unsigned short type)
** �������� ��д����λ�Ĵ�����
** ��ڲ��� ��freq��ʹ�õ�Ƶ�ʼĴ�����
							phase��ʹ�õ���λ�Ĵ�����
							type��Ҫ����Ĳ������͡�
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_Setup(unsigned short freq1,unsigned short freq2, unsigned short phase1,unsigned short phase2,unsigned short type1,unsigned short type2)
{
	unsigned short val1 = 0;
	unsigned short val2 = 0;
	
	val1 = freq1 | phase1 | type1;
	val2 = freq2 | phase2 | type2;
	AD9833_SetRegisterValue(val1,val2);
}

/*****************************************************************************************
** �������� ��void AD9833_SetWave(unsigned short type)
** �������� ������Ҫ����Ĳ������͡�
** ��ڲ��� ��type��Ҫ����Ĳ������͡�
** ���ڲ��� ����
** ����˵�� ����
*******************************************************************************************/
void AD9833_SetWave(unsigned short type1,unsigned short type2)
{
	AD9833_SetRegisterValue(type1,type2);
}


/*********************************************END OF FILE**********************/
