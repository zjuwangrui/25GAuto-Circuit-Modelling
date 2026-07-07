#include "stm32f10x.h"
#include "Delay.h"

uint16_t k;
extern uint16_t fre_set;
extern uint16_t V_set_x10;

uint8_t cmd_wave_head[6] = {0xEE, 0xB1, 0x32, 0x00, 0x01, 0x00};
uint8_t cmd_head[6] = {0xEE, 0xB1, 0x10, 0x00, 0x00, 0x00};
uint8_t cmd_head2[6] = {0xEE, 0xB1, 0x10, 0x00, 0x01, 0x00};
uint8_t cmd_end[4] = {0xFF, 0xFC, 0xFF, 0xFF};
uint8_t Serial_RxPacket[2];	  // 定义接收数据包数组
uint8_t Serial_Screen_RxFlag; // 定义接收数据包标志位
uint8_t Serial_Screen_Fre_Packet[8];
uint8_t Serial_Screen_Fre_BitCount;
uint8_t Serial_Screen_RxFlag2;

void Serial_Screen_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE); // 开启UART4的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 开启GPIOA的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure); // 将PA9引脚初始化为复用推挽输出

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure); // 将PA10引脚初始化为上拉输入

	/*USART初始化*/
	USART_InitTypeDef USART_InitStructure;											// 定义结构体变量
	USART_InitStructure.USART_BaudRate = 9600;										// 波特率
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 硬件流控制，不需要
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;					// 模式，发送模式和接收模式均选择
	USART_InitStructure.USART_Parity = USART_Parity_No;								// 奇偶校验，不需要
	USART_InitStructure.USART_StopBits = USART_StopBits_1;							// 停止位，选择1位
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;						// 字长，选择8位
	USART_Init(UART4, &USART_InitStructure);										// 将结构体变量交给USART_Init，配置UART4

	/*中断输出配置*/
	USART_ITConfig(UART4, USART_IT_RXNE, ENABLE); // 开启串口接收数据的中断

	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 配置NVIC为分组2

	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;					  // 定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;		  // 选择配置NVIC的UART4线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  // 指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; // 指定NVIC线路的抢占优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		  // 指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);							  // 将结构体变量交给NVIC_Init，配置NVIC外设

	/*USART使能*/
	USART_Cmd(UART4, ENABLE);
}

void Serial_Screen_SendByte(uint8_t Byte)
{
	USART_SendData(UART4, Byte); // 将字节数据写入数据寄存器，写入后USART自动生成时序波形
	while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET)
		; // 等待发送完成
	/*下次写入数据寄存器会自动清除发送完成标志位，故此循环后，无需清除标志位*/
}
uint8_t Serial_Screen_GetRxFlag(void)
{
	if (Serial_Screen_RxFlag == 1) // 如果标志位为1
	{
		Serial_Screen_RxFlag = 0;
		return 1; // 则返回1，并自动清零标志位
	}
	return 0; // 如果标志位为0，则返回0
}
uint8_t Serial_Screen_GetRxFlag2(void)
{
	if (Serial_Screen_RxFlag2 == 1) // 如果标志位为1
	{
		Serial_Screen_RxFlag2 = 0;
		return 1; // 则返回1，并自动清零标志位
	}
	return 0; // 如果标志位为0，则返回0
}
void Serial_Screen_clearZone(void);
void UART4_IRQHandler(void)
{
	static uint8_t RxState = 0;	  // 定义表示当前状态机状态的静态变量
	static uint8_t pRxPacket = 0; // 定义表示当前接收数据位置的静态变量
	static uint8_t RxState2 = 0;  // 定义表示当前状态机状态的静态变量
	static uint8_t pRxPacket2 = 0;
	if (USART_GetITStatus(UART4, USART_IT_RXNE) == SET) // 判断是否是UART4的接收事件触发的中断
	{
		uint8_t RxData = USART_ReceiveData(UART4); // 读取数据寄存器，存放在接收的数据变量

		/*使用状态机的思路，依次处理数据包的不同部分*/

		/*当前状态为0，接收数据包包头*/
		if (RxState == 0)
		{
			if (RxData == 0xFE) // 如果数据确实是包头
			{
				RxState = 1;   // 置下一个状态
				pRxPacket = 0; // 数据包的位置归零
			}
		}
		/*当前状态为1，接收数据包数据*/
		else if (RxState == 1)
		{
			Serial_RxPacket[pRxPacket] = RxData; // 将数据存入数据包数组的指定位置
			pRxPacket++;						 // 数据包的位置自增
			if (pRxPacket >= 2)					 //
			{
				RxState = 2; // 置下一个状态
			}
		}
		/*当前状态为2，接收数据包包尾*/
		else if (RxState == 2)
		{
			if (RxData == 0xFF) // 如果数据确实是包尾部
			{
				RxState = 0;			  // 状态归0
				Serial_Screen_RxFlag = 1; // 接收数据包标志位置1，成功接收一个数据包
			}
		}
		if (RxState2 == 0)
		{
			if (RxData == 0x04) // 如果数据确实是包头
			{
				RxState2 = 1; // 置下一个状态
			}
		}
		else if (RxState2 == 1)
		{

			if (RxData == 0x11) // 如果数据确实是包头
			{
				RxState2 = 2;	// 置下一个状态
				pRxPacket2 = 0; // 数据包的位置归零
				Serial_Screen_Fre_BitCount = 0;
			}
		}
		/*当前状态为1，接收数据包数据*/
		else if (RxState2 == 2)
		{
			Serial_Screen_Fre_Packet[pRxPacket2] = RxData; // 将数据存入数据包数组的指定位置
			Serial_Screen_Fre_BitCount++;
			pRxPacket2++;		// 数据包的位置自增
			if (RxData == 0x00) //
			{
				RxState2 = 3; // 置下一个状态
			}
		}
		/*当前状态为2，接收数据包包尾*/
		else if (RxState2 == 3)
		{

			if (RxData == 0xFF) // 如果数据确实是包尾部
			{
				RxState2 = 0;			   // 状态归0
				Serial_Screen_RxFlag2 = 1; // 接收数据包标志位置1，成功接收一个数据包
			}
		}
		USART_ClearITPendingBit(UART4, USART_IT_RXNE); // 清除标志位
	}
}

//=========================================================
void Serial_Screen_SendCmdHead(void)
{
	for (uint8_t i = 0; i < 6; i++)
	{
		Serial_Screen_SendByte(cmd_head[i]);
	}
}

void send_cmd_wave_head(void)
{
	for (uint8_t i = 0; i < 6; i++)
	{
		Serial_Screen_SendByte(cmd_wave_head[i]);
	}
}

void Serial_Screen_SendCmdEnd(void)
{
	for (uint8_t i = 0; i < 4; i++)
	{
		Serial_Screen_SendByte(cmd_end[i]);
	}
}
void Serial_Screen_clearText(uint8_t id)
{
	Serial_Screen_SendCmdHead();
	Serial_Screen_SendByte(id);
	Serial_Screen_SendCmdEnd();
}

void Serial_Screen_setFre(uint8_t id, uint32_t fre)
{
	Serial_Screen_clearText(id);

	if (fre < 1000)
	{
		Serial_Screen_SendCmdHead();
		Serial_Screen_SendByte(id);
		Serial_Screen_SendByte(48 + fre / 100);
		Serial_Screen_SendByte(48 + fre % 100 / 10);
		Serial_Screen_SendByte(48 + fre % 10);
		Serial_Screen_SendCmdEnd();
	}
	else if (fre < 10000)
	{
		Serial_Screen_SendCmdHead();
		Serial_Screen_SendByte(id);
		Serial_Screen_SendByte(48 + fre / 1000);
		Serial_Screen_SendByte(48 + fre % 1000 / 100);
		Serial_Screen_SendByte(48 + fre % 100 / 10);
		Serial_Screen_SendByte(48 + fre % 10);
		Serial_Screen_SendCmdEnd();
	}
	else if (fre < 100000)
	{
		Serial_Screen_SendCmdHead();
		Serial_Screen_SendByte(id);
		Serial_Screen_SendByte(48 + fre / 10000);
		Serial_Screen_SendByte(48 + fre % 10000 / 1000);
		Serial_Screen_SendByte(48 + fre % 1000 / 100);
		Serial_Screen_SendByte(48 + fre % 100 / 10);
		Serial_Screen_SendByte(48 + fre % 10);
		Serial_Screen_SendCmdEnd();
	}
	else if (fre < 1000000)
	{
		Serial_Screen_SendCmdHead();
		Serial_Screen_SendByte(id);
		Serial_Screen_SendByte(48 + fre / 100000);
		Serial_Screen_SendByte(48 + fre % 100000 / 10000);
		Serial_Screen_SendByte(48 + fre % 10000 / 1000);
		Serial_Screen_SendByte(48 + fre % 1000 / 100);
		Serial_Screen_SendByte(48 + fre % 100 / 10);
		Serial_Screen_SendByte(48 + fre % 10);
		Serial_Screen_SendCmdEnd();
	}
	else if (fre < 10000000)
	{
		Serial_Screen_SendCmdHead();
		Serial_Screen_SendByte(id);
		Serial_Screen_SendByte(48 + fre / 1000000);
		Serial_Screen_SendByte(48 + fre % 1000000 / 10000);
		Serial_Screen_SendByte(48 + fre % 100000 / 10000);
		Serial_Screen_SendByte(48 + fre % 10000 / 1000);
		Serial_Screen_SendByte(48 + fre % 1000 / 100);
		Serial_Screen_SendByte(48 + fre % 100 / 10);
		Serial_Screen_SendByte(48 + fre % 10);
		Serial_Screen_SendCmdEnd();
	}
}
void Serial_Screen_setHw(uint8_t id, uint16_t v)
{
	Serial_Screen_clearText(id);
	Serial_Screen_SendCmdHead();
	Serial_Screen_SendByte(id);
	Serial_Screen_SendByte(48 + v / 100);
	Serial_Screen_SendByte(0x2E);
	Serial_Screen_SendByte(48 + v % 100 / 10);
	Serial_Screen_SendByte(48 + v % 10);
	Serial_Screen_SendCmdEnd();
}
void Serial_Screen_setFilterType(uint8_t type)
{
	Serial_Screen_clearText(31);
	if (type == 1)
	{
		Serial_Screen_SendByte(0xEE);
		Serial_Screen_SendByte(0xB1);
		Serial_Screen_SendByte(0x10);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x1F);
		Serial_Screen_SendByte(0xB5);
		Serial_Screen_SendByte(0xCD);
		Serial_Screen_SendByte(0xCD);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC2);
		Serial_Screen_SendByte(0xCB);
		Serial_Screen_SendByte(0xB2);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC6);
		Serial_Screen_SendByte(0xF7);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFC);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFF);
	}
	else if (type == 2)
	{
		Serial_Screen_SendByte(0xEE);
		Serial_Screen_SendByte(0xB1);
		Serial_Screen_SendByte(0x10);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x1F);
		Serial_Screen_SendByte(0xB8);
		Serial_Screen_SendByte(0xDF);
		Serial_Screen_SendByte(0xCD);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC2);
		Serial_Screen_SendByte(0xCB);
		Serial_Screen_SendByte(0xB2);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC6);
		Serial_Screen_SendByte(0xF7);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFC);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFF);
	}
	else if (type == 3)
	{
		Serial_Screen_SendByte(0xEE);
		Serial_Screen_SendByte(0xB1);
		Serial_Screen_SendByte(0x10);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x1F);
		Serial_Screen_SendByte(0xB4);
		Serial_Screen_SendByte(0xF8);
		Serial_Screen_SendByte(0xCD);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC2);
		Serial_Screen_SendByte(0xCB);
		Serial_Screen_SendByte(0xB2);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC6);
		Serial_Screen_SendByte(0xF7);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFC);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFF);
	}
	else if (type == 4)
	{
		Serial_Screen_SendByte(0xEE);
		Serial_Screen_SendByte(0xB1);
		Serial_Screen_SendByte(0x10);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x1F);
		Serial_Screen_SendByte(0xB4);
		Serial_Screen_SendByte(0xF8);
		Serial_Screen_SendByte(0xD7);  // 变化点：0xCD → 0xD7
		Serial_Screen_SendByte(0xE8);  // 变化点：0xA8 → 0xE8
		Serial_Screen_SendByte(0xC2);
		Serial_Screen_SendByte(0xCB);
		Serial_Screen_SendByte(0xB2);
		Serial_Screen_SendByte(0xA8);
		Serial_Screen_SendByte(0xC6);
		Serial_Screen_SendByte(0xF7);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFC);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFF);
	}
	else
	{
		Serial_Screen_SendByte(0xEE);
		Serial_Screen_SendByte(0xB1);
		Serial_Screen_SendByte(0x10);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x00);
		Serial_Screen_SendByte(0x1F);
		Serial_Screen_SendByte(0xC5);  // 变化点
		Serial_Screen_SendByte(0xD0);  // 变化点
		Serial_Screen_SendByte(0xB6);  // 变化点
		Serial_Screen_SendByte(0xCF);  // 变化点
		Serial_Screen_SendByte(0xCA);  // 变化点
		Serial_Screen_SendByte(0xA7);  // 变化点
		Serial_Screen_SendByte(0xB0);  // 变化点
		Serial_Screen_SendByte(0xDC);  // 变化点
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFC);
		Serial_Screen_SendByte(0xFF);
		Serial_Screen_SendByte(0xFF);
	}
	
	
	
}

void Serial_Screen_setVpp(uint8_t id, uint16_t v)
{
	Serial_Screen_clearText(id);
	Serial_Screen_SendCmdHead();
	Serial_Screen_SendByte(id);
	Serial_Screen_SendByte(48 + v / 10);
	Serial_Screen_SendByte(0x2E);
	Serial_Screen_SendByte(48 + v % 10);
	Serial_Screen_SendCmdEnd();
}
