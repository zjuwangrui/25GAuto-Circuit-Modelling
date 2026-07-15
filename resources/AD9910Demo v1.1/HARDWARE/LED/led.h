#ifndef __LED_H
#define __LED_H	 
#include "sys.h"

//LED驱动代码	   

//修改日期:2012/9/2
//版本：V1.0

#define LED0 PBout(5)// PB5
#define LED1 PEout(5)// PE5	

void LED_Init(void);

		 				    
#endif
