/**
  ******************************************************************************
  * @file    main.c
  * @author  南微电子工作室
  * @version V1.0
  * @date    2017-04-024
  * @brief   
  ******************************************************************************
  * @attention
  *
  * 实验平台:STM32F103开发板 
  * 淘宝地址:https://shop129810565.taobao.com
  *
  ******************************************************************************
  */

#include "sys.h"
#include "delay.h"
#include "AD9910_Cfg.h"

int main(){
	int Freq=420000000;
	GPIO_init();
	AD9910_Init();
	Freq_convert(Freq);
	while(1){
		
		
		//Freq+=1000000;
		//Freq_convert(Freq);
		
	}
	return 0;
}


