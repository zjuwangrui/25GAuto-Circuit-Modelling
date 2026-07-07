#ifndef __DAC902_H
#define __DAC902_H	 
#include "sys.h"
/*
		�����ĳ��Լ����ư�����š����罫DAC902_CLK�Ÿĳ�PC2���ƣ�����"#define DAC902_CLK PCin(2)"
		ע�⣡����
		������ɺ����ڡ�void DAC902_IO_Init()���������ʼ����Ӧ������
*/
#define DAC902_CLK 				PAout(4)
#define DAC902_PowerON() PAout(12)=0;
#define DAC902_PowerOFF() PAout(12)=1;

#define ALL_POINT 	200

void DAC902_Init(void);//��ʼ��DAC902ģ��
void Set_UpDown_Point(void);//��ʼ������
void DAC902_WriteData(uint16_t dat);//д����
void Triangle_Wave(void);//�������ǲ�����
void Sine_Wave(void);
void Set_Fre(u32 mfre);
#endif

