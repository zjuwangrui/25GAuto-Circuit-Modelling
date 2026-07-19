#include "LCD.h"

void initscr(uint8_t screen_id){
    cmd_2[0]=0xEE;
    cmd_2[1]=0xB1;
    cmd_2[2]=0x00;
    cmd_2[3]=0x00;
    cmd_2[4]=screen_id;
    cmd_2[5]=0xFF;
    cmd_2[6]=0xFC;
    cmd_2[7]=0xFF;
    cmd_2[8]=0xFF;
}
void initcur(uint8_t screen_id, uint8_t set_ID, uint16_t numofdata){
    cmd_3[0]=0xEE;
    cmd_3[1]=0xB1;
    cmd_3[2]=0x32;
    cmd_3[3]=0x00;
    cmd_3[4]=screen_id;
    cmd_3[5]=0x00;
    cmd_3[6]=set_ID;  //控件ID
    cmd_3[7]=0x00;
    cmd_3[8]=numofdata / 256;
    cmd_3[9]=numofdata % 256; //个数
    cmd_3[10]=0xFF;
    cmd_3[11]=0xFC;
    cmd_3[12]=0xFF;
    cmd_3[13]=0xFF;
}


void changescr(uint8_t screen_id){
    uint8_t k;
    initscr(screen_id);
    for(k=0;k<9;){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd_2[k++]);
    }

}
void initStr(uint8_t screen_id){
    cmd[0]=0xEE;
    cmd[1]=0xB1;
    cmd[2]=0x10;
    cmd[3]=0x00;
    cmd[4]=screen_id;
    cmd[5]=0x00;
    cmd[6]=0x01;
    cmd[7]=0xFF;
    cmd[8]=0xFC;
    cmd[9]=0xFF;
    cmd[10]=0xFF;
       
}

void initStr_SimScr(uint8_t set_ID){
    cmd[0]=0xEE;
    cmd[1]=0xB1;
    cmd[2]=0x10;
    cmd[3]=0x00;
    cmd[4]=0x00;
    cmd[5]=0x00;
    cmd[6]=set_ID;
    cmd[7]=0xFF;
    cmd[8]=0xFC;
    cmd[9]=0xFF;
    cmd[10]=0xFF;
       
}

void initStr_New(uint8_t screen_id, uint8_t set_id){
    cmd[0]=0xEE;
    cmd[1]=0xB1;
    cmd[2]=0x10;
    cmd[3]=0x00;
    cmd[4]=screen_id;
    cmd[5]=0x00;
    cmd[6]=set_id;
    cmd[7]=0xFF;
    cmd[8]=0xFC;
    cmd[9]=0xFF;
    cmd[10]=0xFF;
       
}

void sendStr_New(char *str,uint8_t screen_id,uint8_t set_id){
    uint8_t i;
    initStr_New(screen_id, set_id);
    for(i=0;i<7;){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
    while(*str!='\0'){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, *str++);
    }
    //DL_UART_Main_transmitDataBlocking(UART_0_INST, 0x25);          //发送百分号
    while (i < 11) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
}

void sendStr_SimScr(char *str,uint8_t set_ID){
    uint8_t i;
    initStr_SimScr(set_ID);
    for(i=0;i<7;){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
    while(*str!='\0'){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, *str++);
    }
    //DL_UART_Main_transmitDataBlocking(UART_0_INST, 0x25);          //发送百分号
    while (i < 11) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
}

void sendStr(char *str,uint8_t screen_id){
    uint8_t i;
    initStr(screen_id);
    for(i=0;i<7;){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
    while(*str!='\0'){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, *str++);
    }
    DL_UART_Main_transmitDataBlocking(UART_0_INST, 0x25);          //发送百分号
    while (i < 11) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd[i++]);
    }
}
void sendcurve(volatile int16_t *cur,uint8_t screen_id, uint8_t set_id, uint16_t numofdata){
    uint8_t i;
    uint16_t k=0;
    initcur(screen_id, set_id, numofdata);
    for(i=0;i<10;){
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd_3[i++]);
    }
    while(k<1024){
        DL_UART_Main_transmitDataBlocking(UART_0_INST,(char)(cur[k]/256));
        DL_UART_Main_transmitDataBlocking(UART_0_INST,(char)(cur[k]%256));
        k++;
    }
    while (i < 14) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, cmd_3[i++]);
    }
}

void clearScreen(void){
    sendStr(" ", 1);
    sendStr(" ", 2);
    sendStr(" ", 3);
    sendStr(" ", 4);
    sendStr(" ", 5);
}
void clearcurve(void){
    uint8_t i;
    uint8_t k;
    cmd_4[0]=0xEE;
    cmd_4[1]=0xB1;
    cmd_4[2]=0x33;
    cmd_4[3]=0x00;
    cmd_4[4]=0x00;     //Screen
    cmd_4[5]=0x00;
    cmd_4[6]=0x01;     //控件ID
    cmd_4[7]=0x00;
    cmd_4[8]=0xFF;
    cmd_4[9]=0xFC;
    cmd_4[10]=0xFF;
    cmd_4[11]=0xFF;
    cmd_5[0]=0xEE;
    cmd_5[1]=0xB1;
    cmd_5[2]=0x34;
    cmd_5[3]=0x00;
    cmd_5[4]=0x01;
    cmd_5[5]=0x00;
    cmd_5[6]=0x09;
    cmd_5[7]=0x00;
    cmd_5[8]=0x00;
    cmd_5[9]=0x00;
    cmd_5[10]=0x32;
    cmd_5[11]=0x00;
    cmd_5[12]=0x00;
    cmd_5[13]=0x00;
    cmd_5[14]=0x0A;
    cmd_5[15]=0xFF;
    cmd_5[16]=0xFC;
    cmd_5[17]=0xFF;
    cmd_5[18]=0xFF;
    for(i=1;i<6;i++){
        cmd_4[4]=i;
        cmd_5[4]=i;
        for(k=0;k<12;k++){
            DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_4[k]);
        }
        for(k=0;k<19;k++){
            DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_5[k]);
        }
    }
    
}

void Draw_Line(uint16_t x0,uint16_t y0,uint16_t x1, uint16_t y1)
{
    uint8_t i;
    cmd_6[0] = 0xEE;
    cmd_6[1] = 0x51;
    cmd_6[2] = x0 / 256;
    cmd_6[3] = x0 % 256;
    cmd_6[4] = y0 / 256;
    cmd_6[5] = y0 % 256;
    cmd_6[6] = x1 / 256;
    cmd_6[7] = x1 % 256;
    cmd_6[8] = y1 / 256;
    cmd_6[9] = y1 % 256;
    cmd_6[10] = 0xFF;
    cmd_6[11] = 0xFC;
    cmd_6[12] = 0xFF;
    cmd_6[13] = 0xFF;
    for(i=0;i<14;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_6[i]);
    }
}

void Draw_Dot(uint16_t x,uint16_t y)
{
    uint8_t i;
    cmd_7[0] = 0xEE;
    cmd_7[1] = 0x41;
    cmd_7[2] = 0xF8;
    cmd_7[3] = 0x00;
    cmd_7[4] = 0xFF;
    cmd_7[5] = 0xFC;
    cmd_7[6] = 0xFF;
    cmd_7[7] = 0xFF;
    for(i=0;i<8;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_7[i]);
    }
    cmd_7[0] = 0xEE;
    cmd_7[1] = 0x50;
    cmd_7[2] = x / 256;
    cmd_7[3] = x % 256;
    cmd_7[4] = y / 256;
    cmd_7[5] = y % 256;
    cmd_7[6] = 0xFF;
    cmd_7[7] = 0xFC;
    cmd_7[8] = 0xFF;
    cmd_7[9] = 0xFF;
    for(i=0;i<10;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_7[i]);
    }
}
void clear_curve_dots( void )
{
    uint8_t i;
    cmd_8[0] = 0xEE;
    cmd_8[1] = 0x41;
    cmd_8[2] = 0xFF;
    cmd_8[3] = 0xFF;
    cmd_8[4] = 0xFF;
    cmd_8[5] = 0xFC;
    cmd_8[6] = 0xFF;
    cmd_8[7] = 0xFF;
    for(i=0;i<8;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_8[i]);
    }
    cmd_8[0] = 0xEE;
    cmd_8[1] = 0x55;
    cmd_8[2] = 180 / 256;
    cmd_8[3] = 180 % 256;
    cmd_8[4] = 180 / 256;
    cmd_8[5] = 180 % 256;
    cmd_8[6] = 0x02;
    cmd_8[7] = 0xB3;
    cmd_8[8] = 0x01;
    cmd_8[9] = 0xB3;
    cmd_8[10] = 0xFF;
    cmd_8[11] = 0xFC;
    cmd_8[12] = 0xFF;
    cmd_8[13] = 0xFF;
     for(i=0;i<14;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_8[i]);
    }
}

void get_slider_value(uint8_t set_ID)
{
    cmd_8[0] = 0xEE;
    cmd_8[1] = 0xB1;
    cmd_8[2] = 0x11;
    cmd_8[3] = 0x00;
    cmd_8[4] = 0x00;
    cmd_8[5] = 0x00;
    cmd_8[6] = set_ID;
    cmd_8[7] = 0xFF;
    cmd_8[8] = 0xFC;
    cmd_8[9] = 0xFF;
    cmd_8[10] = 0xFF;
    for(uint8_t i=0;i<11;i++)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST,cmd_8[i]);
    }
    
}