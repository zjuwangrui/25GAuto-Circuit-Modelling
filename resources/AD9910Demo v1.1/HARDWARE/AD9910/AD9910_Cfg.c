#include "AD9910_Cfg.h"

uchar cfr1[]={0x00,0x40,0x00,0x00};       //cfr1控制字
//code uchar cfr3[]={0x1f,0x3f,0xc0,0x00};       //cfr3控制字
uchar cfr3[]={0x05,0x0F,0x41,0x32};       //cfr3控制字  40M输入  25倍频  VC0=101   ICP=001; //40M时0x28改成0x32
uchar profile11[]={0x00,0x00,0x00,0x00,0x25,0x09,0x7b,0x42};       //profile1控制字 0x25,0x09,0x7b,0x42

void GPIO_init(){
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11;  //GPIO_PA00~PA11
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;//推输出模式
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//50MHz
  GPIO_Init(GPIOA, &GPIO_InitStructure);
		
}

void txd_8bit(uchar txdat)
{uchar i,sbt;
  sbt=0x80;
  SCLK=0;
  for (i=0;i<8;i++)
   {if ((txdat & sbt)==0) SDO=0; else SDO=1;
    SCLK=1;
	sbt=sbt>>1;
	SCLK=0;
   }
 } 

void AD9910_Init(){
	uchar k,m;
 PROFILE2=PROFILE1=PROFILE0=0;
 DRCTL=0;DRHOLD=0;
 MASTREST=1;  
 MASTREST=0; 
 CS=0;
 txd_8bit(0x00);    //发送CFR1控制字地址
 for (m=0;m<4;m++)
 txd_8bit(cfr1[m]); 
 CS=1;  
 for (k=0;k<10;k++);

 CS=0;
  txd_8bit(0x02);    //发送CFR3控制字地址
 for (m=0;m<4;m++)
 txd_8bit(cfr3[m]); 
  CS=1;
 for (k=0;k<10;k++);

 UP_DAT=1;
 for(k=0;k<10;k++);
 UP_DAT=0;
}

void Txfrc(void)
{uchar m;

 CS=0;
txd_8bit(0x0e);    //发送profile1控制字地址
 for (m=0;m<8;m++)
 txd_8bit(profile11[m]); 
 CS=1;
// for(k=0;k<10;k++);


 UP_DAT=1;
// for(k=0;k<10;k++);
 UP_DAT=0;
// Delay_ms(1);
} 

void Freq_convert(ulong Freq)
{
	  ulong Temp;            
	  Temp=(ulong)Freq*4.294967296;	   //将输入频率因子分为四个字节  4.294967296=(2^32)/1000000000
	  profile11[7]=(uchar)Temp;
	  profile11[6]=(uchar)(Temp>>8);
	  profile11[5]=(uchar)(Temp>>16);
	  profile11[4]=(uchar)(Temp>>24);
	  Txfrc();
}

