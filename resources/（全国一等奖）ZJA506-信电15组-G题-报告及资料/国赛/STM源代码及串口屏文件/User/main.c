#include "stm32f10x.h" // Device header
#include "AD.h"
#include "Serial.h"
#include "Delay.h"
#include "Serial_Screen.h"
#include "AD9910.h"
#include "fft.h"
#include "filter.h"
#include "math.h"
#include "DAC.h"
#include "arm_math.h"
#include "Relay.h"
#include "IC.h"

#define TYPE_LOWPASS 1
#define TYPE_HIGHPASS 2
#define TYPE_BANDPASS 3
#define TYPE_BANDSTOP 4
uint8_t filter_type;

float Q = 1.0 / 3.0;
float w0 = 10000.0;
uint32_t fre_set = 200;
uint16_t V_set_x10 = 10;
float V_dds_f;
uint32_t V_dds;
uint32_t amp;
// 用于扫频的数列
uint32_t fres_saopin[530] = {100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 129, 132, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 230, 235, 240, 245, 250, 255, 260, 265, 270, 275, 280, 286, 292, 298, 304, 310, 316, 322, 328, 335, 342, 349, 356, 363, 370, 377, 385, 393, 401, 409, 417, 425, 434, 443, 452, 461, 470, 479, 489, 499, 509, 519, 529, 540, 551, 562, 573, 584, 596, 608, 620, 632, 645, 658, 671, 684, 698, 712, 726, 741, 756, 771, 786, 802, 818, 834, 851, 868, 885, 903, 921, 939, 958, 977, 997, 1017, 1037, 1058, 1079, 1101, 1123, 1145, 1168, 1191, 1215, 1239, 1264, 1289, 1315, 1341, 1368, 1395, 1423, 1451, 1480, 1510, 1540, 1571, 1602, 1634, 1667, 1700, 1734, 1769, 1804, 1840, 1877, 1915, 1953, 1992, 2032, 2073, 2114, 2156, 2199, 2243, 2288, 2334, 2381, 2429, 2478, 2528, 2579, 2631, 2684, 2738, 2793, 2849, 2906, 2964, 3023, 3083, 3145, 3208, 3272, 3337, 3404, 3472, 3541, 3612, 3684, 3758, 3833, 3910, 3988, 4068, 4149, 4232, 4317, 4403, 4491, 4581, 4673, 4766, 4861, 4958, 5057, 5158, 5261, 5366, 5473, 5582, 5694, 5808, 5924, 6042, 6163, 6286, 6412, 6540, 6671, 6804, 6940, 7079, 7221, 7365, 7512, 7662, 7815, 7971, 8130, 8293, 8459, 8628, 8801, 8977, 9157, 9340, 9527, 9718, 9912, 10110, 10312, 10518, 10728, 10943, 11162, 11385, 11613, 11845, 12082, 12324, 12570, 12821, 13077, 13339, 13606, 13878, 14156, 14439, 14728, 15023, 15323, 15629, 15942, 16261, 16586, 16918, 17256, 17601, 17953, 18312, 18678, 19052, 19433, 19822, 20218, 20622, 21034, 21455, 21884, 22322, 22768, 23223, 23687, 24161, 24644, 25137, 25640, 26153, 26676, 27210, 27754, 28309, 28875, 29452, 30041, 30642, 31255, 31880, 32518, 33168, 33831, 34508, 35198, 35902, 36620, 37352, 38099, 38861, 39638, 40431, 41240, 42065, 42906, 43764, 44639, 45532, 46443, 47372, 48319, 49285, 50271, 51276, 52302, 53348, 54415, 55503, 56613, 57745, 58900, 60078, 61280, 62506, 63756, 65031, 66332, 67659, 69012, 70392, 71800, 73236, 74701, 76195, 77719, 79273, 80858, 82475, 84124, 85806, 87522, 89272, 91057, 92878, 94736, 96631, 98564, 100535, 105535, 110535, 115535, 120535, 125535, 130535, 135535, 140535, 145535, 150535, 155535, 160535, 165535, 170535, 175535, 180535, 185535, 190535, 195535, 200535, 205535, 210535, 215535, 220535, 225535, 230535, 235535, 240535, 245535, 250535, 255535, 260535, 265535, 270535, 275535, 280535, 285535, 290535, 295535, 300535, 305535, 310535, 315535, 320535, 325535, 330535, 335535, 340535, 345535, 350535, 355535, 360535, 365535, 370535, 375535, 380535, 385535, 390535, 395535, 400535, 405535, 410535, 415535, 420535, 425535, 430535, 435535, 440535, 445535, 450535, 455535, 460535, 465535, 470535, 475535, 480535, 485535, 490535, 495535, 500535, 505535, 510535, 515535, 520535, 525535, 530535, 535535, 540535, 545535, 550535, 555535, 560535, 565535, 570535, 575535, 580535, 585535, 590535, 595535, 600535, 605535, 610535, 615535, 620535, 625535, 630535, 635535, 640535, 645535, 650535, 655535, 660535, 665535, 670535, 675535, 680535, 685535, 690535, 695535, 700535, 705535, 710535, 715535, 720535, 725535, 730535, 735535, 740535, 745535, 750535, 755535, 760535, 765535, 770535, 775535, 780535, 785535, 790535, 795535, 800535, 805535, 810535, 815535, 820535, 825535, 830535, 835535, 840535, 845535, 850535, 855535, 860535, 865535, 870535, 875535, 880535, 885535, 890535, 895535, 900535, 905535, 910535, 915535, 920535, 925535, 930535, 935535, 940535, 945535, 950535, 955535, 960535, 965535, 970535, 975535, 980535, 985535, 990535, 995535};

// 频点总数
uint16_t num_saopin = 530;

// 最大增益
float max_gain;
uint16_t max_gain_index;
// 最小增益
float min_gain;
uint16_t min_gain_index;
// 当前增益
float gain;
float gains[205];
float high_gain;

float low_gain;
float jiezhi_gain;
float f_low = 0, f_high = 0;
uint16_t cutoff_index;
float freq1;
float freq2;
float gain_fc_db;
float gain_f2_db;
float slope;

float nihe_Q = 0.0f;
float nihe_w0 = 0.0f;
float nihe_f0 = 0.0f;
float nihe_K = 0.0f;
float a2, a1, a0, b2, b1, b0;

float32_t filter_in[1024], filter_out[1024];
uint16_t dac_data[1024];
uint16_t dac_clk;

uint16_t ii;

float getGain(uint32_t fre);
float getHw(float fre);

float d_a  = 0.0f;
int main()
{
	Serial_Init();
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	ADC_DMA_Init(); // 初始化ADC的DMA中断
	AD_Init();		// 初始化ADC
	TIM8_init(180, 1);
	Relay_Init();
	Serial_Screen_Init();
    IC_Init();
	// 	Delay_ms(500);
	//  AD9833_Init();
	// AD9833_SetFrequencyQuick(50000.0, AD9833_OUT_TRIANGLE); // 写输出频率1000.0Hz,输出正弦波
	// //	AD9833_SetPhase(AD9833_REG_PHASE0,2048);//设置180°相位，范围：0-4095(0-360°)
	GPIO_init();
	AD9910_Init();
	Freq_Amp_convert(10000, 300);


	Relay_On();
	while (1)
	{

		if (Serial_Screen_GetRxFlag())
		{
			if (Serial_RxPacket[0] == 0xC1)
			{
				d_a += 0.02f;
			}
			if (Serial_RxPacket[0] == 0xC0)
			{
				d_a -= 0.02f;
			}
			if (Serial_RxPacket[0] == 0xA1)
			{
				if (Serial_RxPacket[1] == 0xA0)
				{
					fre_set -= 100;
				}
				else if (Serial_RxPacket[1] == 0xA1)
				{
					fre_set += 100;
				}
				else if (Serial_RxPacket[1] == 0xA2)
				{
					fre_set = 0;

					for (ii = 0; ii < Serial_Screen_Fre_BitCount - 1; ii++)
					{

						fre_set = 10 * fre_set + (Serial_Screen_Fre_Packet[ii] - 0x30);
					}
				}

				if (fre_set == 100)
				{
					amp = 600;
				}
				else if (fre_set == 200)
				{
					amp = 400;
				}
				else if (fre_set == 300)
				{
					amp = 350;
				}
				else if (fre_set == 400)
				{
					amp = 330;
				}
				else
					amp = 300;

				Serial_Screen_setFre(4, fre_set);
				AD9910_Init();
				Freq_Amp_convert(fre_set, amp);
			}

			if (Serial_RxPacket[0] == 0xA2)
			{
				if (Serial_RxPacket[1] == 0xA0)
				{
					V_set_x10 -= 1;
				}
				else if (Serial_RxPacket[1] == 0xA1)
				{
					V_set_x10 += 1;
				}

				Serial_Screen_setVpp(7, V_set_x10);
			}

			if (Serial_RxPacket[0] == 0xA0)
			{
				V_dds_f = ((float)V_set_x10 * 100.0 / getHw(fre_set));
				V_dds_f /= getGain(fre_set); // 这里修改为放大倍数
				V_dds = (int)(V_dds_f + 0.5);
				AD9910_Init();
				Freq_Amp_convert(fre_set, V_dds);
				Serial_Screen_setHw(33, (int)(getHw(fre_set) * 100 + 0.5));
				Serial_Screen_setFre(35, (int)((getGain(fre_set) * (float)V_dds) + 0.5));
				//Serial_Printf("gain:%.2f,Hw:%.2f,freset:%d,vddsf:%.2f,vdds:%d\n\r\n", getGain(fre_set), getHw(fre_set), fre_set, V_dds_f, V_dds);
			}
			if (Serial_RxPacket[0] == 0xB0)
			{
				max_gain = 0;
				AD9910_Init();
				Freq_Amp_convert(100, 200);
				Delay_ms(300);
				for (uint16_t i = 0; i < num_saopin; i++)
				{
					if (fres_saopin[i] < 500)
					{
						TIM_Cmd(TIM8, DISABLE);
						TIM8_init(1440, 1);
					}
					else
					{
						TIM_Cmd(TIM8, DISABLE);
						TIM8_init(180, 1);
					}

					AD9910_Init();
					Freq_Amp_convert(fres_saopin[i], 200);
					Delay_ms(100);
					adc_flag = 0;
					TIM_Cmd(TIM8, ENABLE);
					while (adc_flag == 0)
						;
					getVpp(AD_Value_buf);

					gain = (float)Vpp / (200 * getGain(fres_saopin[i]));
					gains[i] = gain;
					if (gain > max_gain)
					{
						max_gain = gain;
						max_gain_index = i;
					}
					//Serial_Printf("Vpp:%d,gain:%.2f,fre:%d\n\r\n", Vpp, gain, fres_saopin[i]);
				}
				if (max_gain > 1)
				{
					max_gain = 1;
				}
				jiezhi_gain = max_gain * 0.707f;

				low_gain = (gains[0] + gains[1] + gains[2]) / 3;
				high_gain = (gains[num_saopin - 3] + gains[num_saopin - 2] + gains[num_saopin - 1]) / 3;

				if (low_gain > 0.65 * max_gain && high_gain < 0.65 * max_gain)
				{
					filter_type = TYPE_LOWPASS;
				}
				else if (low_gain < 0.6 * max_gain && high_gain > 0.65 * max_gain)
				{
					filter_type = TYPE_HIGHPASS;
				}
				else if (low_gain < 0.6 * max_gain && high_gain < 0.6 * max_gain)
				{
					filter_type = TYPE_BANDPASS;
				}
				else if (low_gain > 0.5 * max_gain && high_gain > 0.5 * max_gain)
				{
					filter_type = TYPE_BANDSTOP;
				}

				if (filter_type == TYPE_BANDPASS)
				{
					for (uint16_t i = max_gain_index; i > 16; i--)
					{
						if (gains[i] >= jiezhi_gain && gains[i - 1] < jiezhi_gain)
						{
							// 线性插值精确计算频率
							f_low = (float)fres_saopin[i - 1] +
									(jiezhi_gain - gains[i - 1]) *
										(fres_saopin[i] - fres_saopin[i - 1]) /
										(gains[i] - gains[i - 1]);
							break;
						}
					}
					for (uint16_t i = max_gain_index; i < 500 - 1; i++)
					{
						if (gains[i] >= jiezhi_gain && gains[i + 1] < jiezhi_gain)
						{
							f_high = (float)fres_saopin[i] +
									 (jiezhi_gain - gains[i]) *
										 (fres_saopin[i + 1] - fres_saopin[i]) /
										 (gains[i] - gains[i + 1]);
							break;
						}
					}
					nihe_f0 = sqrtf(f_low * f_high);
					nihe_w0 = 2 * PI * nihe_f0;
					nihe_Q = nihe_f0 / (f_high - f_low);
					nihe_K = max_gain;
					a1 = nihe_K * nihe_w0 / nihe_Q;
					a2 = 0;
					a0 = 0;
					b2 = 1;
					b1 = nihe_w0 / nihe_Q;
					b0 = nihe_w0 * nihe_w0;
				}
				else if (filter_type == TYPE_BANDSTOP)
				{
					// 1. 寻找最小增益点（中心频率点）

					min_gain_index = 0;
					min_gain = gains[0];
					for (uint16_t i = 1; i < 175; i++)
					{
						if (gains[i] < min_gain)
						{
							min_gain = gains[i];
							min_gain_index = i;
						}
					}

					// 2. 初始化截止频率
					f_low = 0.0f;
					f_high = 0.0f;

					// 3. 向低频方向搜索截止点（增益上升沿）
					for (uint16_t i = min_gain_index; i > 1; i--)
					{
						if (gains[i] <= jiezhi_gain && gains[i - 1] > jiezhi_gain)
						{
							// 线性插值计算低频截止频率
							f_low = (float)fres_saopin[i - 1] +
									(jiezhi_gain - gains[i - 1]) *
										(fres_saopin[i] - fres_saopin[i - 1]) /
										(gains[i] - gains[i - 1]);
							break;
						}
					}

					// 4. 向高频方向搜索截止点（增益上升沿）
					for (uint16_t i = min_gain_index; i < 500; i++) // 174=175-1
					{
						if (gains[i] <= jiezhi_gain && gains[i + 1] > jiezhi_gain)
						{
							// 线性插值计算高频截止频率
							f_high = (float)fres_saopin[i] +
									 (jiezhi_gain - gains[i]) *
										 (fres_saopin[i + 1] - fres_saopin[i]) /
										 (gains[i + 1] - gains[i]);
							break;
						}
					}

					// 5. 计算滤波器关键参数
					nihe_f0 = sqrtf(f_low * f_high);	 // 中心频率
					nihe_w0 = 2 * PI * nihe_f0;			 // 角频率
					nihe_Q = nihe_f0 / (f_high - f_low); // 品质因数
					nihe_K = max_gain;					 // 通带增益

					// 6. 计算传递函数系数（双二阶形式）
					// 分子系数: K*(s^2 + w0^2)
					a2 = nihe_K;					 // s^2项系数
					a1 = 0;							 // s项系数
					a0 = nihe_K * nihe_w0 * nihe_w0; // 常数项系数

					// 分母系数: s^2 + (w0/Q)s + w0^2
					b2 = 1;					// s^2项系数（归一化）
					b1 = nihe_w0 / nihe_Q;	// s项系数
					b0 = nihe_w0 * nihe_w0; // 常数项系数
				}
				else if (filter_type == TYPE_LOWPASS)
				{
					// 1. 寻找截止频率点（下降沿）

					for (uint16_t i = 0; i < 500; i++)
					{
						if (gains[i] >= jiezhi_gain && gains[i + 1] < jiezhi_gain)
						{
							// 线性插值计算截止频率
							f_high = (float)fres_saopin[i] +
									 (jiezhi_gain - gains[i]) *
										 (fres_saopin[i + 1] - fres_saopin[i]) /
										 (gains[i] - gains[i + 1]);
							break;
						}
					}
					nihe_f0 = f_high;
					nihe_K = max_gain;
					nihe_w0 = 2 * PI * nihe_f0; // 角频率
					nihe_Q = 0.707;
					a2 = 0;
					a1 = 0;
					a0 = nihe_K * nihe_w0 * nihe_w0;
					b2 = 1;
					b1 = nihe_w0 / nihe_Q;
					b0 = nihe_w0 * nihe_w0;
				}
				else if (filter_type == TYPE_HIGHPASS)
				{

					// 1. 寻找截止频率点（上升沿）

					for (uint16_t i = 500; i > 0; i--)
					{
						if (gains[i] >= jiezhi_gain && gains[i - 1] < jiezhi_gain)
						{
							// 线性插值计算截止频率
							f_low = (float)fres_saopin[i - 1] +
									(jiezhi_gain - gains[i - 1]) *
										(fres_saopin[i] - fres_saopin[i - 1]) /
										(gains[i] - gains[i - 1]);
							break;
						}
					}
					nihe_f0 = f_low;
					nihe_w0 = 2 * PI * nihe_f0; // 角频率
					nihe_Q = 0.707;
					nihe_K = max_gain; // 高频增益
					// 4. 传递函数系数（二阶高通）
					a2 = nihe_K; // s^2项系数
					a1 = 0;
					a0 = 0;

					b2 = 1;
					b1 = nihe_w0 / nihe_Q;
					b0 = nihe_w0 * nihe_w0;
				}
				Serial_Printf("max_gain:%.2f,high_gain:%.2f,low_gain:%.2f,filter_type:%d,f_low:%.2f,f_high:%.2f\n\r\n", max_gain, high_gain, low_gain, filter_type, f_low, f_high);
				Serial_Screen_setFilterType(filter_type);
				Serial_Screen_setFre(42, (int)f_low);
				Serial_Screen_setFre(43, (int)f_high);
			}

			if (Serial_RxPacket[0] == 0xB1)
			{
				Relay_Off();
				
				initIIRFilter(a2, a1, a0, b2, b1, b0, IC_GetFreq() * 16);
				
				 AD_SwitchToEXTI();
				Serial_Printf("freq:%d\n\r\n",IC_GetFreq());
					
					adc_flag = 0;
					while (adc_flag == 0)
						;
					for (ii = 0; ii < 1024; ii++)
					{
						filter_in[ii] = (float)AD_Value_buf[ii] - Vdc;
						//Serial_Printf("%.2f\n", filter_in[ii]);
					}
					processIIRFilter(filter_in, filter_out, 1024);
					for (ii = 0; ii < 1024; ii++)
					{
						dac_data[ii] = (int)(filter_out[ii] + 2048);
						//Serial_Printf("%d\n", dac_data[ii]);
					}
					DAC_Mode_Init(dac_data);
				
			}
		}
	}
}
float getHw(float fre)
{
	float w = 2 * PI * fre;
	return 5.0 /
		   sqrt((1 - (w / w0) * (w / w0)) * (1 - (w / w0) * (w / w0)) + (w / (w0 * Q)) * (w / (w0 * Q)));
}
float getGain(uint32_t fre)
{
	if (fre > 50 && fre <= 150)
	{
		return 4.40 + d_a;
	}
	else if (fre > 150 && fre <= 250)
	{
		return 7.8+ d_a;
	}
	else if (fre > 250 && fre <= 350)
	{
		return 10.25+ d_a;
	}
	else if (fre > 350 && fre <= 450)
	{
		return 11.4+ d_a;
	}
	else if (fre > 450 && fre <= 550)
	{
		return 12.4+ d_a;
	}
	else if (fre > 550 && fre <= 650)
	{
		return 12.9+ d_a;
	}
	else if (fre > 650 && fre <= 750)
	{
		return 13.35+ d_a;
	}
	else if (fre > 750 && fre <= 850)
	{
		return 13.6+ d_a;
	}
	else
		return 13.65;
}
