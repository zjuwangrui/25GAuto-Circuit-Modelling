#include "AD9910.h"

uchar cfr1[]={0x00,0x41,0x00,0x00};       //cfr1??????
//code uchar cfr3[]={0x1f,0x3f,0xc0,0x00};       //cfr3??????
uchar cfr2[]={0x01,0x40,0x08,0x20};       //cfr2
uchar cfr3[]={0x05,0x0F,0x41,0x32};       //cfr3??????  40M????  25???  VC0=101   ICP=001; //40M?0x28???0x32
uchar profile11[]={0x3F,0xFF,0x00,0x00,0x25,0x09,0x7b,0x42};       //profile1?????? 0x25,0x09,0x7b,0x42

uchar drgparameter[20]={0x00}; //DRG can shu
uchar ramprofile0[8] = {0x00}; //ramprofile0 kong zhi zi
//extern const unsigned char ramdata_Square[4096];



void GPIO_init(){
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_7;  //A7 A5
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;//???????
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//50MHz
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_8|GPIO_Pin_13|GPIO_Pin_15;//B 1 8  13 15
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_7|GPIO_Pin_6; //C4 6 7
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; //E6
	GPIO_Init(GPIOE, &GPIO_InitStructure);

}

void txd_8bit(uchar txdat)
{uchar i,sbt;
  sbt=0x80;
  SCLK=0;
  for (i=0;i<8;i++)
   {if ((txdat & sbt)==0) SDO=0; else SDO=1;
    SCLK=1;
	sbt=sbt>>1;
	SCLK=0;
   }
 } 

void AD9910_Init(){
	uchar k,m;
 PROFILE2=PROFILE1=PROFILE0=0;
 DRCTL=0;DRHOLD=0;
 MASTREST=1;  
 MASTREST=0; 
 CS=0;
 txd_8bit(0x00);    //????CFR1????????
 for (m=0;m<4;m++)
 txd_8bit(cfr1[m]); 
 CS=1;  
 for (k=0;k<10;k++);

	/////
	CS=0;
 txd_8bit(0x01);    //????CFR2????????
 for (m=0;m<4;m++)
 txd_8bit(cfr2[m]); 
 CS=1;  
 for (k=0;k<10;k++);
	/////
	
	
	
 CS=0;
  txd_8bit(0x02);    //????CFR3????????
 for (m=0;m<4;m++)
 txd_8bit(cfr3[m]); 
  CS=1;
 for (k=0;k<10;k++);

 UP_DAT=1;
 for(k=0;k<10;k++);
 UP_DAT=0;
}

void Txfrc(void)
{uchar m;

 CS=0;
txd_8bit(0x0e);    //????profile1????????
 for (m=0;m<8;m++)
 txd_8bit(profile11[m]); 
 CS=1;
// for(k=0;k<10;k++);


 UP_DAT=1;
// for(k=0;k<10;k++);
 UP_DAT=0;
// Delay_ms(1);
} 

void Freq_Amp_convert(ulong Freq,ulong Amp)
{
	 ulong Temp;            
	  Temp=(ulong)Freq*4.294967296;	   //?????????????????????  4.294967296=(2^32)/1000000000
	  profile11[7]=(uchar)Temp;
	  profile11[6]=(uchar)(Temp>>8);
	  profile11[5]=(uchar)(Temp>>16);
	  profile11[4]=(uchar)(Temp>>24);
	  

    Temp = (ulong)Amp*22.2608696;           //?????????????  23.405714=(2^14)/736
    if(Temp > 0x3fff)
    Temp = 0x3fff;
    Temp &= 0x3fff;
		profile11[1]=(uchar)Temp;
    profile11[0]=(uchar)(Temp>>8);
		
		Txfrc();
}
//======================ad9910  DRG==========================
void Txdrg(void)
{
        uchar m,k;

        CS=0;
        txd_8bit(0x0b);    //??????????0x0b
        for (m=0;m<8;m++)
                txd_8bit(drgparameter[m]); 
        CS=1;
        for(k=0;k<10;k++);
        
        CS=0;
        txd_8bit(0x0c);    //??????????0x0c
        for (m=8;m<16;m++)
                txd_8bit(drgparameter[m]); 
        CS=1;
        for(k=0;k<10;k++);
        
        CS=0;
        txd_8bit(0x0d);    //??????????0x0d
        for (m=16;m<20;m++)
                txd_8bit(drgparameter[m]); 
        CS=1;
        for(k=0;k<10;k++);
        
        UP_DAT=1;
        for(k=0;k<10;k++);
        UP_DAT=0;
       // delay_ms(1);
}         

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
        ITemp4 = (ulong)SweepTime/4; //1GHz/4, ??:ns
        if(ITemp4 > 0xffff)
                ITemp4 = 0xffff;
        DTemp4 = ITemp4;
        
        //sao pin shang xia xian
        drgparameter[7]=(uchar)Temp1;
        drgparameter[6]=(uchar)(Temp1>>8);
        drgparameter[5]=(uchar)(Temp1>>16);
        drgparameter[4]=(uchar)(Temp1>>24);
        drgparameter[3]=(uchar)Temp2;
        drgparameter[2]=(uchar)(Temp2>>8);
        drgparameter[1]=(uchar)(Temp2>>16);
        drgparameter[0]=(uchar)(Temp2>>24);
        //pinlv bu jin(??:Hz)
        drgparameter[15]=(uchar)ITemp3;
        drgparameter[14]=(uchar)(ITemp3>>8);
        drgparameter[13]=(uchar)(ITemp3>>16);
        drgparameter[12]=(uchar)(ITemp3>>24);
        drgparameter[11]=(uchar)DTemp3;
        drgparameter[10]=(uchar)(DTemp3>>8);
        drgparameter[9]=(uchar)(DTemp3>>16);
        drgparameter[8]=(uchar)(DTemp3>>24);
        //shi jian jian ge(??:us)
        drgparameter[19]=(uchar)ITemp4;
        drgparameter[18]=(uchar)(ITemp4>>8);
        drgparameter[17]=(uchar)DTemp4;
        drgparameter[16]=(uchar)(DTemp4>>8);
        //??DRG??
        Txdrg();
}

//=================ad9910 ramprofile0 kong zhi zi cheng xu=====================
void Txramprofile(void)
{
        uchar m,k;

        CS=0;
        txd_8bit(0x0e);    // fa song ramprofile0 kong zhi zi di zhi 
        for (m=0;m<8;m++)
                txd_8bit(ramprofile0[m]); 
        CS=1;
        for(k=0;k<10;k++);

        UP_DAT=1;
        for(k=0;k<10;k++);
        UP_DAT=0;
        //delay_ms(1);
}         
//=====================================================================

////=======================ad9910 fa song ramdata cheng xu=========================
//void Txramdata(void)
//{
//        uchar m,k;

//        CS=0;
//        txd_8bit(0x16);    //  ram kong zhi zi
//        for (m=0; m<4096; m++)
//				{
//					txd_8bit(ramdata_Square[m]); 
//				}
//				CS=1;
//        for(k=0;k<10;k++);

//        UP_DAT=1;
//        for(k=0;k<10;k++);
//        UP_DAT=0;
//        //delay_ms(1);
//}         
//=====================================================================

////======================= fang bo can shu she zhi ========================
//void Square_wave(uint Sample_interval)//??
//{
//        ulong Temp;
//        Temp = Sample_interval/4; //1GHz/4, ??????:4*(1~65536)ns
//        if(Temp > 0xffff)
//                Temp = 0xffff;
//        ramprofile0[7] = 0x24;
//        ramprofile0[6] = 0x00;
//        ramprofile0[5] = 0x00;
//        ramprofile0[4] = 0xc0;
//        ramprofile0[3] = 0x0f;
//        ramprofile0[2] = (uchar)Temp;
//        ramprofile0[1] = (uchar)(Temp>>8);
//        ramprofile0[0] = 0x00;
//        Txramprofile();

//        Txramdata();        
//}
//=====================================================================

