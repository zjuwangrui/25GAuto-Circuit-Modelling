#ifndef __SERIAL_SCREEN_H
#define __SERIAL_SCREEN_H

extern uint8_t Serial_RxPacket[2];
extern uint8_t Serial_Screen_Fre_Packet[8];
extern uint8_t Serial_Screen_Fre_BitCount;
void Serial_Screen_Init(void);
uint8_t Serial_Screen_GetRxFlag(void);
void Serial_Screen_Draw(void);

void Serial_Screen_setPhaseDiff(uint8_t id,  uint16_t phase);
void Serial_Screen_DrawSqWave(uint8_t id,uint16_t duty);
void Serial_Screen_setFre(uint8_t id,uint32_t fre);
void Serial_Screen_setVpp(uint8_t id, uint16_t v);
void Serial_Screen_setHw(uint8_t id, uint16_t Hw);
uint8_t Serial_Screen_GetRxFlag2(void);
void Serial_Screen_setFilterType(uint8_t type);


#endif

