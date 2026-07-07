#ifndef __AD9833_H
#define	__AD9833_H

#include"stm32f10x.h"

/******************************************************************************/
/* AD9833                                                                    */
/******************************************************************************/
/* 寄存器 */

#define AD9833_REG_CMD		(0 << 14)
#define AD9833_REG_FREQ0	(1 << 14)
#define AD9833_REG_FREQ1	(2 << 14)
#define AD9833_REG_PHASE0	(6 << 13)
#define AD9833_REG_PHASE1	(7 << 13)

/* 命令控制位 */

#define AD9833_B28				(1 << 13)
#define AD9833_HLB				(1 << 12)
#define AD9833_FSEL0			(0 << 11)
#define AD9833_FSEL1			(1 << 11)
#define AD9833_PSEL0			(0 << 10)
#define AD9833_PSEL1			(1 << 10)
#define AD9833_PIN_SW			(1 << 9)
#define AD9833_RESET			(1 << 8)
#define AD9833_SLEEP1			(1 << 7)
#define AD9833_SLEEP12		(1 << 6)
#define AD9833_OPBITEN		(1 << 5)
#define AD9833_SIGN_PIB		(1 << 4)
#define AD9833_DIV2				(1 << 3)
#define AD9833_MODE				(1 << 1)

#define AD9833_OUT_SINUS		((0 << 5) | (0 << 1) | (0 << 3))//正弦波 
#define AD9833_OUT_TRIANGLE	((0 << 5) | (1 << 1) | (0 << 3))//三角波
#define AD9833_OUT_MSB			((1 << 5) | (0 << 1) | (1 << 3)) //方波
#define AD9833_OUT_MSB2			((1 << 5) | (0 << 1) | (0 << 3))

#define AD9833_GPIO_PORT             GPIOB

#define AD9833_GPIO_RCC              RCC_APB2Periph_GPIOB

#define AD9833_1_GPIO_D              GPIO_Pin_8
#define AD9833_GPIO_CLK              GPIO_Pin_14
#define AD9833_GPIO_SYNC             GPIO_Pin_12

#define AD9833_2_GPIO_D              GPIO_Pin_9
//#define AD9833_2_GPIO_CLK            GPIO_Pin_15
//#define AD9833_2_GPIO_SYNC           GPIO_Pin_13

#define ad9833_1_D_h                 GPIO_SetBits(AD9833_GPIO_PORT,AD9833_1_GPIO_D)
#define ad9833_CLK_h                 GPIO_SetBits(AD9833_GPIO_PORT,AD9833_GPIO_CLK)
#define ad9833_SYNC_h                GPIO_SetBits(AD9833_GPIO_PORT,AD9833_GPIO_SYNC)
#define ad9833_1_D_l                 GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_1_GPIO_D)
#define ad9833_CLK_l                 GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_GPIO_CLK)
#define ad9833_SYNC_l                GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_GPIO_SYNC)

#define ad9833_2_D_h                 GPIO_SetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_D)
#define ad9833_2_D_l                 GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_D)

//#define ad9833_2_CLK_h               GPIO_SetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_CLK)
//#define ad9833_2_SYNC_h              GPIO_SetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_SYNC)
//#define ad9833_2_CLK_l               GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_CLK)
//#define ad9833_2_SYNC_l              GPIO_ResetBits(AD9833_GPIO_PORT,AD9833_2_GPIO_SYNC)

void AD9833_GPIO_Init(void);//初始化IO口
void AD9833_Init(void);//初始化IO口及寄存器

void AD9833_Reset(void);			//置位AD9833的复位位
void AD9833_ClearReset(void);	//清除AD9833的复位位

void AD9833_SetRegisterValue(unsigned short regValue1,unsigned short regValue2);												//将值写入寄存器
void AD9833_SetFrequency(unsigned short reg, float fout1,unsigned short type1,float fout2,unsigned short type2);	//写入频率寄存器
void AD9833_SetPhase(unsigned short reg, unsigned short val1, unsigned short val2);									//写入相位寄存器

void AD9833_Setup(unsigned short freq1,unsigned short freq2, unsigned short phase1,unsigned short phase2,unsigned short type1,unsigned short type2);//选择频率、相位和波形类型
void AD9833_SetFrequencyQuick(float fout1,unsigned short type1,float fout2,unsigned short type2); //设置频率及波形类型


#endif /* __KEY_H */
