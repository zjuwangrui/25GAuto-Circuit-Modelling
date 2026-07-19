#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdlib.h>
#define UART_PACKET_SIZE (2)


//串口通信
#define LEN 20
volatile uint8_t cmd[LEN];
volatile uint8_t cmd_2[LEN];
volatile uint8_t cmd_3[LEN];
volatile uint8_t cmd_4[LEN];
volatile uint8_t cmd_5[LEN];
volatile uint8_t cmd_6[LEN];
volatile uint8_t cmd_7[LEN];
volatile uint8_t cmd_8[LEN];

//函数声明
void sendString(uint8_t *str);
void initStr(uint8_t screen_id);
void sendStr(char *str,uint8_t screen_id);
void clearScreen(void);
void initscr(uint8_t screen_id);
void changescr(uint8_t screen_id);
void initcur(uint8_t screen_id, uint8_t set_ID, uint16_t numofdata);
void sendcurve(volatile int16_t *cur,uint8_t screen_id,uint8_t set_id,uint16_t numofdata);
void clearcurve(void);
void initStr_SimScr(uint8_t screen_id);
void sendStr_SimScr(char *str,uint8_t set_ID);
void Draw_Line(uint16_t x0,uint16_t y0,uint16_t x1, uint16_t y1);
void Draw_Dot(uint16_t x,uint16_t y);
void clear_curve_dots( void );
void initStr_New(uint8_t screen_id, uint8_t set_id);
void sendStr_New(char *str,uint8_t screen_id,uint8_t set_id);