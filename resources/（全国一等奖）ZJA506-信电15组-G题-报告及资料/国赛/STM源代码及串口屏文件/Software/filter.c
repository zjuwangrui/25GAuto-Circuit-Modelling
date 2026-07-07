#include "arm_math.h"
#include <math.h>
#include "stm32f10x.h"
#include "Serial.h"

#define M_PI 3.1415926

#define SAMPLE_RATE    400000.0f
#define LOWER_CUTOFF   900.0f
#define UPPER_CUTOFF   1100.0f

// IIR滤波器结构体
arm_biquad_casd_df1_inst_f32 iirInstance;
float32_t iirCoeffs[5];   // [b0, b1, b2, a1, a2]
float32_t iirState[4];    // 状态缓存 (二阶滤波器需要4个状态变量)


float32_t coeffs[5] = {0.63150322,-1.25071776,0.63150322,1.25071776,-0.26300648};



 // {b0, b1, b2, a1, a2}
float32_t c;
float32_t c2;
float32_t bb0;
float32_t bb1;
float32_t bb2;
float32_t aa0;
float32_t aa1;
float32_t aa2;
void initIIRFilter(float a2,float a1,float a0,float b2,float b1,float b0,uint32_t fs) 
{
    c = 2 * (float)fs;
    c2 = c*c;
    bb0 = b2 * c2 + b1 * c + b0;
    bb1 = (-2)*b2*c2 + 2*b0;
    bb2 = b2 * c2 -b1 * c + b0;
    
    aa0 = a2 * c2 + a1 * c + a0;
    aa1 = (-2)*a2*c2 + 2*a0;
    aa2 = a2 * c2 -a1 * c + a0;

    aa0 /= bb0;
    aa1 /= bb0;
    aa2 /= bb0;
    bb1 /= bb0;
    bb2 /= bb0;

    coeffs[0] = aa0;
    coeffs[1] = aa1;
    coeffs[2] = aa2;
    coeffs[3] = -bb1;
    coeffs[4] = -bb2;
    Serial_Printf("{%.8f,%.8f,%.8f,%.8f,%.8f};\n\r\n",coeffs[0],coeffs[1],coeffs[2],coeffs[3],coeffs[4]);

    
    // 6. 初始化IIR滤波器
    arm_biquad_cascade_df1_init_f32(&iirInstance, 1, coeffs, iirState);
}

// 使用滤波器处理数据
void processIIRFilter(float32_t *input, float32_t *output, uint32_t blockSize) {
    arm_biquad_cascade_df1_f32(&iirInstance, input, output, blockSize);
}

