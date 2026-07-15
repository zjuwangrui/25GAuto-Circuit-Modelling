#ifndef __KEY_H__
#define __KEY_H__

#include "sys.h"

#define KEY_UP 		4
#define KEY_LEFT	3
#define KEY_DOWN	2
#define KEY_RIGHT	1

//#define KEY0 PEin(4)	//PE4
//#define KEY1 PEin(3)	//PE3 
//#define KEY2 PEin(2)	//PE2
//#define KEY3 PAin(0)	//PA0  WK_UP

#define KEY0  GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_4)//读取按键0
#define KEY1  GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_3)//读取按键1
#define KEY2  GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_2)//读取按键2 
#define KEY3  GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)//读取按键3(WK_UP)

/*@func		按键初始化	
 *
 *@param	void	
 *
 *@return	void	
 */
void KEY_Init(void);

/*@func		按键扫描	
 *
 *@param	扫描模式
 *
 *@return	void	
 */
u8 KEY_Scan(u8 mode);

#endif
