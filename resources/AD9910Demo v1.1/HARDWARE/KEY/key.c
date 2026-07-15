#include "key.h"
#include "delay.h"

/*@func		按键初始化	
 *
 *@param	void	
 *
 *@return	void	
 */
void KEY_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOE, ENABLE);	//ENABLE PA PE时钟
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		//上拉输入PE2.3.4
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;		//下拉输入PA0
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*@func		按键扫描	
 *
 *@param	扫描模式0——不支持连续按 1——支持连续按
 *
 *@return	void	
 */
u8 KEY_Scan(u8 mode)
{
	static u8 key_up = 1;	//按键松开标志
	if(mode)
	{
		key_up = 1;			//支持连按
	}
	if(key_up && (KEY0==0 || KEY1==0 || KEY2==0 || KEY3 == 1))
	{
		delay_ms(10);
		key_up = 0;
		if(KEY0==0) 		return KEY_RIGHT;
		else if(KEY1==0)	return KEY_DOWN;
		else if(KEY2==0)	return KEY_LEFT;
		else if(KEY3==0)	return KEY_UP;
	}
	else if(KEY0==1 && KEY1==1 && KEY2==1 && KEY3==0)
	{
		key_up = 1;
	}
	return 0;	//无按键按下
}

