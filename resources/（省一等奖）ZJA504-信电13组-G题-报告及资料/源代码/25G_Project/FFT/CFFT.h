#ifndef CFFT_H
#define CFFT_H

#include "arm_math.h"
#include "arm_const_structs.h"
#include "math.h"

// FFT配置参数
#define CFFT_NUM_SAMPLES    1024    // FFT采样点数
//#define CFFT_MAX_FREQ_INDEX 5      // 最大频率分量索引（根据应用调整）

// FFT处理结构体
typedef struct {
    q15_t *input;          // 输入数据指针
    q15_t *complexOutput;  // 复数输出缓冲区
    q15_t *magnitude;       // 幅度输出缓冲区
    uint32_t maxIndex;      // 最大幅度索引
    q15_t maxValue;         // 最大幅度值
} CFFT_Handle;

static q15_t hammingWindow[CFFT_NUM_SAMPLES];  // 静态存储窗系数（避免重复计算）

// 函数声明
//void CFFT_Init(CFFT_Handle *handle, q15_t *inputBuffer);
void CFFT_ProcessADCData(CFFT_Handle *handle, uint16_t *adcData, uint32_t dataSize);
void CFFT_ProcessADCData_hamming(CFFT_Handle *handle, uint16_t *adcData, uint32_t dataSize);
void CFFT_Compute(CFFT_Handle *handle);
void CFFT_InitHammingWindow(void);
void CFFT_InitHammingWindow_f32(void);

void CFFT_ComputeIFFT(CFFT_Handle *handle);
void CFFT_f32(float32_t *InputSample, float32_t *ComplexBuffer, float32_t *Mag_Buffer);
void CFFT_f32_Only_Complex(float32_t *InputSample, float32_t *ComplexBuffer);
void CIFFT_f32(float32_t *InputComplexSample, float32_t *Time_Buffer);
void adc_to_cfft_q15(const uint16_t* adc_samples, 
                     q15_t* fft_output, 
                     uint32_t fft_size);
void adc_cifft_q15(q15_t* freq_data_q15, q15_t* time_data_q15, uint32_t fft_size);
void generate_waveform_directly(CFFT_Handle* handle,q15_t *Complex_Buffer, q15_t target_peak_amplitude, q63_t *max_value);
#endif // CFFT_H