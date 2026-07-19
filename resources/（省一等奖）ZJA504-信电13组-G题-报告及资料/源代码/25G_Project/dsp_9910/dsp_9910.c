#include "dsp_9910.h"

static uint16_t Cal_Gain(void);

// 控制字配置 (保持与STM32相同)
uchar cfr1[] = {0x00, 0x40, 0x00, 0x00};
uchar cfr2[] = {0x01, 0x00, 0x00, 0x00};
const uchar cfr3[] = {0x05, 0x0F, 0x41, 0x32}; // 40MHz输入/25倍频
uchar profile11[] = {0x3F,0xFF,0x00,0x00,0x25,0x09,0x7b,0x42};
uchar drgparameter[20]={0x00}; //DRG参数
uchar ramprofile0[8] = {0x00}; //ramprofile0控制字

const unsigned char ramdata_Square[4096];

//高14位幅度控制
const unsigned char ramdata_Square[4096] = {	
//方波
0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 
0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 
0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 
0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00, 0xff,0xfc,0x00,0x00,

0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,


};

//高14位幅度控制
const unsigned char ramdata_Sawtooth[4096] = {
//锯齿波
0x00,0x00,0x00,0x00, 0x03,0xfc,0x00,0x00, 0x07,0xf8,0x00,0x00, 0x0b,0xf4,0x00,0x00, 0x0f,0xf0,0x00,0x00, 0x13,0xec,0x00,0x00, 0x17,0xe8,0x00,0x00, 0x1b,0xe4,0x00,0x00, 
0x1f,0xe0,0x00,0x00, 0x23,0xdc,0x00,0x00, 0x27,0xd8,0x00,0x00, 0x2b,0xd4,0x00,0x00, 0x2f,0xd0,0x00,0x00, 0x33,0xcc,0x00,0x00, 0x37,0xc8,0x00,0x00, 0x3b,0xc4,0x00,0x00, 
0x3f,0xc0,0x00,0x00, 0x43,0xbc,0x00,0x00, 0x47,0xb8,0x00,0x00, 0x4b,0xb4,0x00,0x00, 0x4f,0xb0,0x00,0x00, 0x53,0xac,0x00,0x00, 0x57,0xa8,0x00,0x00, 0x5b,0xa4,0x00,0x00, 
0x5f,0xa0,0x00,0x00, 0x63,0x9c,0x00,0x00, 0x67,0x98,0x00,0x00, 0x6b,0x94,0x00,0x00, 0x6f,0x90,0x00,0x00, 0x73,0x8c,0x00,0x00, 0x77,0x88,0x00,0x00, 0x7b,0x84,0x00,0x00, 	

0x7f,0x80,0x00,0x00, 0x83,0x7c,0x00,0x00, 0x87,0x78,0x00,0x00, 0x8b,0x74,0x00,0x00, 0x8f,0x70,0x00,0x00, 0x93,0x6c,0x00,0x00, 0x97,0x68,0x00,0x00, 0x9b,0x64,0x00,0x00, 
0x9f,0x60,0x00,0x00, 0xa3,0x5c,0x00,0x00, 0xa7,0x58,0x00,0x00, 0xab,0x54,0x00,0x00, 0xaf,0x50,0x00,0x00, 0xb3,0x4c,0x00,0x00, 0xb7,0x48,0x00,0x00, 0xbb,0x44,0x00,0x00, 
0xbf,0x40,0x00,0x00, 0xc3,0x3c,0x00,0x00, 0xc7,0x38,0x00,0x00, 0xcb,0x34,0x00,0x00, 0xcf,0x30,0x00,0x00, 0xd3,0x2c,0x00,0x00, 0xd7,0x28,0x00,0x00, 0xdb,0x24,0x00,0x00, 
0xdf,0x20,0x00,0x00, 0xe3,0x1c,0x00,0x00, 0xe7,0x18,0x00,0x00, 0xeb,0x14,0x00,0x00, 0xef,0x10,0x00,0x00, 0xf3,0x0c,0x00,0x00, 0xf7,0x08,0x00,0x00, 0xfb,0x04,0x00,0x00,


};

// ====================== 64点三角波 (高14位幅度控制) ======================
const unsigned char ramdata_Triangle_64[4096] = {
// 32点上升, 32点下降
0x00,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x10,0x00,0x00,0x00, 0x18,0x00,0x00,0x00, 0x20,0x00,0x00,0x00, 0x28,0x00,0x00,0x00, 0x30,0x00,0x00,0x00, 0x38,0x00,0x00,0x00, 
0x40,0x00,0x00,0x00, 0x48,0x00,0x00,0x00, 0x50,0x00,0x00,0x00, 0x58,0x00,0x00,0x00, 0x60,0x00,0x00,0x00, 0x68,0x00,0x00,0x00, 0x70,0x00,0x00,0x00, 0x78,0x00,0x00,0x00, 
0x80,0x00,0x00,0x00, 0x88,0x00,0x00,0x00, 0x90,0x00,0x00,0x00, 0x98,0x00,0x00,0x00, 0xa0,0x00,0x00,0x00, 0xa8,0x00,0x00,0x00, 0xb0,0x00,0x00,0x00, 0xb8,0x00,0x00,0x00, 
0xc0,0x00,0x00,0x00, 0xc8,0x00,0x00,0x00, 0xd0,0x00,0x00,0x00, 0xd8,0x00,0x00,0x00, 0xe0,0x00,0x00,0x00, 0xe8,0x00,0x00,0x00, 0xf0,0x00,0x00,0x00, 0xf8,0x00,0x00,0x00, 

0xff,0xfc,0x00,0x00, 0xf8,0x00,0x00,0x00, 0xf0,0x00,0x00,0x00, 0xe8,0x00,0x00,0x00, 0xe0,0x00,0x00,0x00, 0xd8,0x00,0x00,0x00, 0xd0,0x00,0x00,0x00, 0xc8,0x00,0x00,0x00, 
0xc0,0x00,0x00,0x00, 0xb8,0x00,0x00,0x00, 0xb0,0x00,0x00,0x00, 0xa8,0x00,0x00,0x00, 0xa0,0x00,0x00,0x00, 0x98,0x00,0x00,0x00, 0x90,0x00,0x00,0x00, 0x88,0x00,0x00,0x00, 
0x80,0x00,0x00,0x00, 0x78,0x00,0x00,0x00, 0x70,0x00,0x00,0x00, 0x68,0x00,0x00,0x00, 0x60,0x00,0x00,0x00, 0x58,0x00,0x00,0x00, 0x50,0x00,0x00,0x00, 0x48,0x00,0x00,0x00, 
0x40,0x00,0x00,0x00, 0x38,0x00,0x00,0x00, 0x30,0x00,0x00,0x00, 0x28,0x00,0x00,0x00, 0x20,0x00,0x00,0x00, 0x18,0x00,0x00,0x00, 0x10,0x00,0x00,0x00, 0x08,0x00,0x00,0x00
};

void __delay_cycles(uint16_t ms){
    delay_cycles(ms*32000);
}

// 软件SPI发送单字节
void txd_8bit(uchar txdat) {
    uchar sbt = 0x80;  // 高位优先(10000000)
    SCLK_low();
    for(uchar i=0; i<8; i++) {
        if((txdat & sbt) == 0)
            SDIO_low() ;
        else
            SDIO_high();
        SCLK_high();
        sbt = sbt >> 1;
        
        SCLK_low();
    }
}
void Txcfr(void)
{
    uchar k,m;

    CS_low();
    txd_8bit(0x00);
    for(m=0;m<4;m++)  txd_8bit(cfr1[m]);
    CS_high();

    for(k=0;k<10;k++);

    CS_low();
    txd_8bit(0x01);
    for(m=0;m<4;m++) txd_8bit(cfr2[m]);
    CS_high();
    for(k=0;k<10;k++);

    CS_low();
    txd_8bit(0x02);
    for(m=0;m<4;m++) txd_8bit(cfr3[m]);
    CS_high();
    for(k=0;k<10;k++);

    UP_DAT_high();
    for(k=0;k<10;k++);
    UP_DAT_low();
    __delay_cycles(1);
}
// AD9910初始化
void AD9910_Init() {
    // 初始化引脚状态
    

   // PWR_low();
    PROFILE2_low();
    PROFILE1_low();
    PROFILE0_low();
    DRCTL_low();
    DRHOLD_low();

    MASTREST_high();
    __delay_cycles(5);
    MASTREST_low();
    Txcfr();
}

// 发送频率配置
void Txfrc(void) {
    uchar m,k;
    CS_low();
    txd_8bit(0x0E);  // Profile1地址
    for(uchar m=0; m<8; m++) txd_8bit(profile11[m]);
    CS_high();
    for(k=0;k<10;k++);
    // 更新触发
    UP_DAT_high();
  
    for(k=0;k<10;k++);
    UP_DAT_low();
    __delay_cycles(1);
}

// 频率转换计算
void Freq_convert(float Freq) {
    ulong Temp = (ulong)(Freq * 4.294967296); // 2^32 / 1e9
    if(Freq > 400000000)
		Freq = 400000000;
    // 填充频率调谐字(小端序)
    profile11[4] = (uchar)(Temp >> 24);
    profile11[5] = (uchar)(Temp >> 16);
    profile11[6] = (uchar)(Temp >> 8);
    profile11[7] = (uchar)Temp;
    
    Txfrc();
    // cfr1[0] = 0x00; // RAM 关闭
    // Txcfr();

    // --- 修改部分 ---
    // 在配置CFR寄存器时，启用逆Sinc滤波器
    cfr1[0] = 0x00; // 保持RAM关闭
    // 比特22位于cfr1[1]中。0x40 = 0b01000000，将比特22置1
    cfr1[1] = 0x40; // 启用逆Sinc滤波器 [cite: 2377]
    cfr1[2] = 0x00;
    cfr1[3] = 0x00;

    Txcfr(); // 发送CFRx控制字

}

void Write_Amplitude(uint Amp)
{
    ulong Temp;
    Temp = (ulong) Amp * 25.20615385;
    if(Temp > 0x3FFF) Temp = 0x3FFF;
    Temp &= 0x3FFF;
    profile11[1] = (uchar) Temp;
    profile11[0] = (uchar) (Temp >> 8);
    Txfrc();
}

void Write_Phi(uint64_t Phi)
{
    uint64_t Temp;
    Temp = (uint64_t) Phi * 65536/360;
    profile11[3] = (uint8_t) Temp;
    profile11[2] = (uint8_t) (Temp >> 8);
    Txfrc();
}

void Txdrg()
{
    uchar m,k;

    CS_low();
    txd_8bit(0x0B);
    for(m=0;m<8;m++) txd_8bit(drgparameter[m]);
    CS_high();
    for(k=0;k<10;k++);

    CS_low();
    txd_8bit(0x0C);
    for (m=8;m<16;m++) txd_8bit(drgparameter[m]); 
    CS_high();
    for(k=0;k<10;k++);

    CS_low();
    txd_8bit(0x0D);
    for (m=16;m<20;m++) txd_8bit(drgparameter[m]); 
    CS_high();
    for(k=0;k<10;k++);

    UP_DAT_high();
    for(k=0;k<10;k++);
    UP_DAT_low();
    __delay_cycles(1);
}

void Txramprofile()
{
    uchar m,k;

    CS_low();
    txd_8bit(0x0e);
    for(m=0;m<8;m++)  txd_8bit(ramprofile0[m]);
    CS_high();
    for(k=0;k<10;k++);
    UP_DAT_high();
    for(k=0;k<10;k++);
    UP_DAT_low();
    __delay_cycles(1);
}


void Txramdata(int wave_num, int Amp)
{
    if(Amp > 200) Amp = 200;
	if(wave_num==1)
	{
        uint m,k;
        CS_low();
        txd_8bit(0x16);                                    //发送ram控制字地址
        for (m=0; m<4096; m++)
		    txd_8bit(ramdata_Square[m] * Amp / 200); 
        CS_high();
        for(k=0;k<10;k++);
        UP_DAT_high();
        for(k=0;k<10;k++);
        UP_DAT_low();
        __delay_cycles(1);
	}
	else if(wave_num == 2)
	{
		uint m,k;
        CS_low();
        txd_8bit(0x16);                                   //发送ram控制字地址
        for(m=0; m<4096; m++)
		    txd_8bit(ramdata_Sawtooth[m] * Amp / 200); 
        CS_high();
        for(k=0;k<10;k++);
        UP_DAT_high();
        for(k=0;k<10;k++);
        UP_DAT_low();
        __delay_cycles(1);
	}
	else{
		uint m,k;
        CS_low();
        txd_8bit(0x16);                                   //发送ram控制字地址
        for(m=0; m<4096; m++)
			txd_8bit(ramdata_Triangle_64[m] * Amp / 200); 
        CS_high();
        for(k=0;k<10;k++);
        UP_DAT_high();
        for(k=0;k<10;k++);
        UP_DAT_low();
        __delay_cycles(1);
	
	}
}   
 //1GHz/4, 采样间隔范围：4*(1~65536)ns
//========================产生方波的程序======================================
void Square_wave(uint Sample_interval, int Amp)
{
        ulong Temp;

	    Temp = ((1000000000/(unsigned long int)(Sample_interval)/64/4));        //1GHz/4, 采样间隔范围：4*(1~65536)ns
        if(Temp > 0xffff)
                Temp = 0xffff;
        ramprofile0[7] = 0x24;
        ramprofile0[6] = 0x00;
        ramprofile0[5] = 0x00;
        ramprofile0[4] = 0xc0;
        ramprofile0[3] = 0x0f;
        ramprofile0[2] = (uchar)Temp;
        ramprofile0[1] = (uchar)(Temp>>8);
        ramprofile0[0] = 0x00;
        Txramprofile();
		Txramdata(1, Amp); 
        cfr1[0] = 0xc0;                                    //RAM 使能，幅度控制
	    cfr2[1] = 0x00;                                              //DRG 失能
	    Txcfr();                                               //发送cfrx控制字				
}

void Square_wave_DutyCycle(float ratio)
{
    ulong Temp = ((1000000000/(unsigned long int)(1000)/64/4));    
    ramprofile0[7] = 0x24;
    ramprofile0[6] = 0x00;
    ramprofile0[5] = 0x00;
    ramprofile0[4] = 0xc0;
    ramprofile0[3] = 0x0f;
    ramprofile0[2] = (uchar)Temp;
    ramprofile0[1] = (uchar)(Temp>>8);
    ramprofile0[0] = 0x00;
    Txramprofile();

    uint m,k;
    CS_low();
    txd_8bit(0x16);                                    //发送ram控制字地址
    for (m=0; m< 64 * ratio; m++)
	{
        txd_8bit(0xFF); 
        txd_8bit(0xFC); 
        txd_8bit(0x00); 
        txd_8bit(0x00); 
    }
    for (; m < 64; m++)
	{
        txd_8bit(0x00); 
        txd_8bit(0x00); 
        txd_8bit(0x00); 
        txd_8bit(0x00); 
    }
    CS_high();
    for(k=0;k<10;k++);
    UP_DAT_high();
    for(k=0;k<10;k++);
    UP_DAT_low();
    __delay_cycles(1);

    cfr1[0] = 0xc0;                                    //RAM 使能，幅度控制
	cfr2[1] = 0x00;                                              //DRG 失能
	Txcfr();
}

//========================产生锯齿波的程序=====================================
void Sawtooth_wave(uint Sample_interval, int Amp)
{
        ulong Temp;

	    Temp = ((1000000000/(unsigned long int)(Sample_interval)/64/4));         //1GHz/4, 采样间隔范围：4*(1~65536)ns
        if(Temp > 0xffff)
                Temp = 0xffff;
        ramprofile0[7] = 0x24;
        ramprofile0[6] = 0x00;
        ramprofile0[5] = 0x00;
        ramprofile0[4] = 0xc0;
        ramprofile0[3] = 0x0f;
        ramprofile0[2] = (uchar)Temp;
        ramprofile0[1] = (uchar)(Temp>>8);
        ramprofile0[0] = 0x00;
        Txramprofile();
		Txramdata(2, Amp); 
        cfr1[0] = 0xc0;                                    //RAM 使能，幅度控制
	    cfr2[1] = 0x00;                                              //DRG 失能
		Txcfr();                                               //发送cfrx控制字				
}

void Triangle_wave(uint Sample_interval, int Amp)
{
        ulong Temp;

	    Temp = ((1000000000/(unsigned long int)(Sample_interval)/64/4));         //1GHz/4, 采样间隔范围：4*(1~65536)ns
        if(Temp > 0xffff)
                Temp = 0xffff;
        ramprofile0[7] = 0x24;
        ramprofile0[6] = 0x00;
        ramprofile0[5] = 0x00;
        ramprofile0[4] = 0xc0;
        ramprofile0[3] = 0x0f;
        ramprofile0[2] = (uchar)Temp;
        ramprofile0[1] = (uchar)(Temp>>8);
        ramprofile0[0] = 0x00;
        Txramprofile();
		Txramdata(3, Amp); 
        cfr1[0] = 0xc0;                                    //RAM 使能，幅度控制
	    cfr2[1] = 0x00;                                              //DRG 失能
		Txcfr();  
                                                     //发送cfrx控制字				
}

//=====================扫频波参数设置和发送程序===================================
void SweepFre(ulong SweepMinFre, ulong SweepMaxFre, ulong SweepStepFre, ulong SweepTime)
{
	ulong Temp1, Temp2, ITemp3, DTemp3, ITemp4, DTemp4;
	Temp1 = (ulong)SweepMinFre*4.294967296;
	if(SweepMaxFre > 400000000)
		SweepMaxFre = 400000000;
	Temp2 = (ulong)SweepMaxFre*4.294967296;
	if(SweepStepFre > 400000000)
		SweepStepFre = 400000000;
	ITemp3 = (ulong)SweepStepFre*4.294967296;
	DTemp3 = ITemp3;
	ITemp4 = (ulong)SweepTime/4;                                    //1GHz/4, 单位：ns
	if(ITemp4 > 0xffff)
		ITemp4 = 0xffff;
	DTemp4 = ITemp4;
	
	//扫频上下限
	drgparameter[7]=(uchar)Temp1;
	drgparameter[6]=(uchar)(Temp1>>8);
	drgparameter[5]=(uchar)(Temp1>>16);
	drgparameter[4]=(uchar)(Temp1>>24);
	drgparameter[3]=(uchar)Temp2;
	drgparameter[2]=(uchar)(Temp2>>8);
	drgparameter[1]=(uchar)(Temp2>>16);
	drgparameter[0]=(uchar)(Temp2>>24);
	//频率步进（单位：Hz）
	drgparameter[15]=(uchar)ITemp3;
	drgparameter[14]=(uchar)(ITemp3>>8);
	drgparameter[13]=(uchar)(ITemp3>>16);
	drgparameter[12]=(uchar)(ITemp3>>24);
	drgparameter[11]=(uchar)DTemp3;
	drgparameter[10]=(uchar)(DTemp3>>8);
	drgparameter[9]=(uchar)(DTemp3>>16);
	drgparameter[8]=(uchar)(DTemp3>>24);
	//步进时间间隔（单位：us）
	drgparameter[19]=(uchar)ITemp4;
	drgparameter[18]=(uchar)(ITemp4>>8);
	drgparameter[17]=(uchar)DTemp4;
	drgparameter[16]=(uchar)(DTemp4>>8);
	//发送DRG参数
	Txdrg();
	cfr1[0] = 0x00; //RAM 失能
	cfr2[1] = 0x0e; //DRG 使能
	Txcfr(); //发送cfrx控制字
}


/**
  * @brief  输出任意波形（基于浮点数数组）
  * @param  freq: 波形重复频率(Hz)
  * @param  samples: 浮点数组（范围0.0~1.0，代表最小到最大幅度）
  * @param  num_points: 数组长度（最大1024）
  * @retval 无
  */
void OutputArbitraryWaveform_NN(float freq, float* samples, int num_points)
{
    // 新增：强制设置辅助DAC（地址0x03）
    const uint8_t DAC_CODE = 0x7F; // 默认满幅值
    
    CS_low();
    txd_8bit(0x03);      // 辅助DAC地址
    txd_8bit(0x00);      // 未使用
    txd_8bit(0x00);      // 未使用
    txd_8bit(0x00);      // 未使用
    txd_8bit(DAC_CODE);  // FSC<7:0>
    CS_high();
    
    // 触发独立I/O更新（确保立即生效）
    __delay_cycles(10);
    UP_DAT_high();
    __delay_cycles(10);
    UP_DAT_low();
    __delay_cycles(10);

    // 1. 参数校验和步进速率计算
    if (num_points > 1024) num_points = 1024;
    if (num_points <= 0) return;

     // 根据数据手册公式计算地址步进速率 (M) [cite: 1691]
    // 波形频率 = (播放速率) / 采样点数 = (f_SYSCLK / (4 * M)) / num_points
    // M = f_SYSCLK / (4 * freq * num_points)
     // 根据您的 cfr3 配置，f_SYSCLK = 1 GHz (1e9 Hz) [cite: 2510]
    uint32_t M = (uint32_t)(1000000000.0 / (4.0 * freq * num_points));
    if (M > 0xFFFF) M = 0xFFFF;  // M 是一个16位的值 [cite: 1621]
    if (M == 0) M = 1;          // M 不能为0

    // 2. 正确配置 RAM Profile 0 (地址 0x0E)
     // 严格参考数据手册表30 (8字节RAM Profile寄存器格式) [cite: 2457]
    // 数组按照MSB优先写入，ramprofile0[0] 对应比特 63:56
    uint16_t start_addr = 0;
    uint16_t end_addr = num_points - 1;

    ramprofile0[0] = 0x00;                                      // 字节 0 (Bits 63:56): 未使用 [cite: 2458]
    ramprofile0[1] = (uint8_t)(M >> 8);                          // 字节 1 (Bits 55:48): 地址步进速率<15:8> [cite: 2458]
    ramprofile0[2] = (uint8_t)(M & 0xFF);                        // 字节 2 (Bits 47:40): 地址步进速率<7:0> [cite: 2458]
    ramprofile0[3] = (uint8_t)(end_addr >> 2);                   // 字节 3 (Bits 39:32): 波形结束地址<9:2> [cite: 2458]
    ramprofile0[4] = (uint8_t)((end_addr & 0x03) << 6) |          // 字节 4 (Bits 31:24): 结束地址<1:0> 和 起始地址<9:2> [cite: 2458]
                     (uint8_t)(start_addr >> 4);
    ramprofile0[5] = (uint8_t)((start_addr & 0x0F) << 4);         // 字节 5 (Bits 23:16): 起始地址<3:0> [cite: 2458]
    ramprofile0[6] = 0x00;                                       // 字节 6 (Bits 15:8): 未使用 [cite: 2458]
    ramprofile0[7] = 0b00000100;                                 // 字节 7 (Bits 7:0): 模式控制 = 100 (Continuous Recirculate 连续循环模式) [cite: 1720, 2458]

    Txramprofile(); // 发送修正后的RAM profile配置

    // 3. 转换浮点采样点并格式化到RAM buffer
    // 幅度模式 (CFR1<30:29>=10)下，数据来自RAM的比特 [31:18] [cite: 1701]
    uchar ram_buffer[4096] = {0};
    const int bytes_per_sample = 4;

    for (int i = 0; i < num_points; i++) {
        // 将浮点采样值 (如 0.0-3.3) 转换为14位幅度值 (0-16383)
        uint16_t amp_value = (uint16_t)(samples[i] / 3.3 * 16383.0f);
        amp_value &= 0x3FFF; // 确保是14位

        // 组装32位RAM数据。将14位的amp_value放入比特 [31:18]
        uint32_t ram_word = (uint32_t)amp_value << 18;

        int offset = i * bytes_per_sample;
        ram_buffer[offset]     = (ram_word >> 24) & 0xFF; // MSB
        ram_buffer[offset + 1] = (ram_word >> 16) & 0xFF;
        ram_buffer[offset + 2] = (ram_word >> 8) & 0xFF;
        ram_buffer[offset + 3] = ram_word & 0xFF;         // LSB
    }

    // 4. 将波形数据写入AD9910的RAM
    CS_low();
    txd_8bit(0x16);  // RAM数据写入命令 (地址 0x16) [cite: 2363]
    for (int i = 0; i < num_points * bytes_per_sample; i++) {
        txd_8bit(ram_buffer[i]);
    }
    CS_high();

    // 5. 触发I/O更新以使设置生效
    __delay_cycles(1); // 满足CS拉高后的时序要求
    UP_DAT_high();
    __delay_cycles(1);  // 满足I/O_UPDATE脉宽要求 [cite: 2278]
    UP_DAT_low();
    __delay_cycles(1);

    // 6. 使用正确的RAM目标模式来使能RAM
    cfr1[0] = 0xC0;  // RAM使能(Bit 31=1), RAM目标=幅度(Bits 30:29 = 10) [cite: 2377, 1701]
    cfr2[1] = 0x00;  // 确保DRG(数字斜坡)功能被禁用 [cite: 2556]
    Txcfr();        // 发送最终配置
}
