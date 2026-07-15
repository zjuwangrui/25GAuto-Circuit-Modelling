#ifndef __AD9910_CFG_H
#define __AD9910_CFG_H

#include "sys.h"

#define uchar unsigned char
#define ulong unsigned long int

#define SCLK PAout(0)
#define SDO PAout(1)      //SDIO
#define CS PAout(2)       //CSB

#define UP_DAT PAout(3)   //UPD
#define MASTREST PAout(4) //RST  

//#define OSK PAout(5)      //NULL
#define DRHOLD PAout(6)   //DRH
#define DRCTL PAout(7)    //DRC
//#define DROVER PAout(8)   //NULL

#define PROFILE2 PAout(9)  //PR2
#define PROFILE1 PAout(10) //PR1
#define PROFILE0 PAout(5) //PR0

void GPIO_init();
void txd_8bit(uchar txdat);
void AD9910_Init();
void Txfrc(void);
void Freq_convert(ulong Freq);

#endif