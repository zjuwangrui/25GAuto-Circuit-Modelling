#ifndef AD9910_CFG_H_
#define AD9910_CFG_H_

#include "ti_msp_dl_config.h"

// 数据类型简化
#define uchar unsigned char
#define ulong unsigned long
#define uint unsigned int


//扫频参数设置
#define START_FREQ     1   // 起始频率
#define END_FREQ       512000 // 终止频率
#define FREQ_STEP      (int16_t)((END_FREQ - START_FREQ) / SWEEP_POINTS)
#define SWEEP_POINTS   512

#define START_FREQ_LF     0   // 起始频率
#define END_FREQ_LF       500000 // 终止频率
#define FREQ_STEP_LF      (int16_t)((END_FREQ_LF - START_FREQ_LF) / SWEEP_POINTS_LF)
#define SWEEP_POINTS_LF   512

// 引脚操作宏 (替代STM32的PAout)
#define SCLK_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_SCLK_PORT, GPIO_AD9910_PIN_SCLK_PIN)
#define SCLK_high() DL_GPIO_setPins(GPIO_AD9910_PIN_SCLK_PORT, GPIO_AD9910_PIN_SCLK_PIN)

#define SDO_low()   DL_GPIO_clearPins(GPIO_AD9910_PIN_SDIO_PORT, GPIO_AD9910_PIN_SDIO_PIN)
#define SDO_high()  DL_GPIO_setPins(GPIO_AD9910_PIN_SDIO_PORT, GPIO_AD9910_PIN_SDIO_PIN)

#define SDIO_low()   DL_GPIO_clearPins(GPIO_AD9910_PIN_SDIO_PORT, GPIO_AD9910_PIN_SDIO_PIN)
#define SDIO_high()  DL_GPIO_setPins(GPIO_AD9910_PIN_SDIO_PORT, GPIO_AD9910_PIN_SDIO_PIN)

#define CS_low()    DL_GPIO_clearPins(GPIO_AD9910_PIN_CS_PORT, GPIO_AD9910_PIN_CS_PIN)
#define CS_high()   DL_GPIO_setPins(GPIO_AD9910_PIN_CS_PORT, GPIO_AD9910_PIN_CS_PIN)

#define UP_DAT_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_UPDATE_PORT, GPIO_AD9910_PIN_UPDATE_PIN)
#define UP_DAT_high() DL_GPIO_setPins(GPIO_AD9910_PIN_UPDATE_PORT, GPIO_AD9910_PIN_UPDATE_PIN)

#define MASTREST_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_RESET_PORT, GPIO_AD9910_PIN_RESET_PIN)
#define MASTREST_high() DL_GPIO_setPins(GPIO_AD9910_PIN_RESET_PORT, GPIO_AD9910_PIN_RESET_PIN)

#define PROFILE0_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_PROFILE0_PORT, GPIO_AD9910_PIN_PROFILE0_PIN)
#define PROFILE0_high() DL_GPIO_setPins(GPIO_AD9910_PIN_PROFILE0_PORT, GPIO_AD9910_PIN_PROFILE0_PIN)

#define PROFILE1_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_PROFILE1_PORT, GPIO_AD9910_PIN_PROFILE1_PIN)
#define PROFILE1_high() DL_GPIO_setPins(GPIO_AD9910_PIN_PROFILE1_PORT, GPIO_AD9910_PIN_PROFILE1_PIN)

#define PROFILE2_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_PROFILE2_PORT, GPIO_AD9910_PIN_PROFILE2_PIN)
#define PROFILE2_high() DL_GPIO_setPins(GPIO_AD9910_PIN_PROFILE2_PORT, GPIO_AD9910_PIN_PROFILE2_PIN)

#define DRCTL_low()   DL_GPIO_clearPins(GPIO_AD9910_PIN_DRCTL_PORT, GPIO_AD9910_PIN_DRCTL_PIN)
#define DRCTL_high()  DL_GPIO_setPins(GPIO_AD9910_PIN_DRCTL_PORT, GPIO_AD9910_PIN_DRCTL_PIN)

#define DRHOLD_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_DRHOLD_PORT, GPIO_AD9910_PIN_DRHOLD_PIN)
#define DRHOLD_high() DL_GPIO_setPins(GPIO_AD9910_PIN_DRHOLD_PORT, GPIO_AD9910_PIN_DRHOLD_PIN)

//#define PWR_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_PWR_PORT, GPIO_AD9910_PIN_PWR_PIN)
//#define PWR_high() DL_GPIO_setPins(GPIO_AD9910_PIN_PWR_PORT, GPIO_AD9910_PIN_PWR_PIN)

#define OSK_low()  DL_GPIO_clearPins(GPIO_AD9910_PIN_OSK_PORT, GPIO_AD9910_PIN_OSK_PIN)
#define OSK_high() DL_GPIO_setPins(GPIO_AD9910_PIN_OSK_PORT, GPIO_AD9910_PIN_OSK_PIN)

// 函数声明
void txd_8bit(uchar txdat);
void AD9910_Init(void);
void Txfrc(void);
void Freq_convert(float Freq);
void Write_Amplitude(uint Amp);
void Write_Phi(uint64_t Phi);

void Square_wave(uint Sample_interval, int Amp);
void Sawtooth_wave(uint Sample_interval, int Amp);
void Triangle_wave(uint Sample_interval, int Amp);
void SweepFre(ulong SweepMinFre, ulong SweepMaxFre, ulong SweepStepFre, ulong SweepTime);
void Square_wave_DutyCycle(float ratio);
void OutputArbitraryWaveform_NN(float freq, float* samples, int num_points);
#endif

