#include "CFFT.h"
// 新增：预计算汉明窗系数数组
static q15_t hammingWindow[CFFT_NUM_SAMPLES];  // 静态存储窗系数（避免重复计算）

// 新增：汉明窗初始化函数（只需调用一次）
void CFFT_InitHammingWindow() {
    for (int i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 汉明窗公式（Q15定点数格式）
        // w(n) = 0.54 - 0.46*cos(2πn/(N-1))
        float window = 0.54f - 0.46f * arm_cos_f32(2 * PI * i / (CFFT_NUM_SAMPLES - 1));
        hammingWindow[i] = (q15_t)(window * 32767);  // 转换为Q15格式
    }
}

void CFFT_InitHammingWindow_f32() {
    for (int i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 汉明窗公式（Q15定点数格式）
        // w(n) = 0.54 - 0.46*cos(2πn/(N-1))
        float window = 0.54f - 0.46f * arm_cos_f32(2 * PI * i / (CFFT_NUM_SAMPLES - 1));
        hammingWindow[i] = window;  // 转换为Q15格式
    }
}

// FFT计算函数
void CFFT_Compute(CFFT_Handle *handle) {
    // 执行FFT（使用文档中的参数配置）
    arm_cfft_q15(
        &arm_cfft_sR_q15_len1024, 
        handle->complexOutput, 
        0,  // IFFTFLAG=0 表示FFT
        1   // BITREVERSE=1 启用位反转
    );
    
    // 计算幅度,精度差
    //结果输出到magnitude
    /*
    arm_cmplx_mag_q15(
        handle->complexOutput,
        handle->magnitude,
        CFFT_NUM_SAMPLES
    );
    */
    q31_t mag_q31[CFFT_NUM_SAMPLES];  // Q31格式的幅度缓存
    // 1. 将Q15复数转换为Q31格式
    q31_t complexOutput_q31[2 * CFFT_NUM_SAMPLES];
    for (int i = 0; i < 2 * CFFT_NUM_SAMPLES; i++) {
        complexOutput_q31[i] = ((q31_t)handle->complexOutput[i]) << 16;  // Q15转Q31
    }
    // 2. 计算Q31精度幅度
    arm_cmplx_mag_q31(
        complexOutput_q31,
        mag_q31,
        CFFT_NUM_SAMPLES
    );
    // 3. 将Q31幅度转换为Q15格式（缩放防止溢出）
    for (int i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 缩放因子：Q31的最大值(2147483647) >> 16 = 32767 (Q15最大值)
        handle->magnitude[i] = (q15_t)(mag_q31[i] >> 16);
    }
    
    // 查找最大幅度值及其索引
    arm_max_q15(
        handle->magnitude + 3, 
        (CFFT_NUM_SAMPLES) / 2 - 3,  // 仅处理前半部分（奈奎斯特频率）
        &handle->maxValue,
        &handle->maxIndex
    );
}


// ADC数据处理函数
void CFFT_ProcessADCData(CFFT_Handle *handle, uint16_t *adcData, uint32_t dataSize) {
    // 0. 确保汉明窗已初始化（只需一次）
    static uint8_t windowInitialized = 0;
    if (!windowInitialized) {
        CFFT_InitHammingWindow();
        windowInitialized = 1;
    }

    // 1. 将12位ADC数据转换为Q15格式
    for (uint32_t i = 0; i < dataSize && i < CFFT_NUM_SAMPLES; i++) {
        // 转换12位ADC到Q15格式：减去直流偏移并缩放
        handle->input[i] = (q15_t)((adcData[i] - 2048) * 8); // -2048~2047 -> -16384~16376
        
        // 构建复数输入（虚部设为0）
        handle->complexOutput[2*i] = handle->input[i]; // 实部
        handle->complexOutput[2*i+1] = 0;              // 虚部
    }
    
    // 2. 填充剩余数据（如果输入不足）
    for (uint32_t i = dataSize; i < CFFT_NUM_SAMPLES; i++) {
        handle->complexOutput[2*i] = 0;
        handle->complexOutput[2*i+1] = 0;

    }
    // // >>>>>>>> 新增：加汉明窗操作（核心位置）<<<<<<<<
    // for (int i = 0; i < CFFT_NUM_SAMPLES; i++) {
    //     // 仅对实部加窗（虚部保持为0）
    //     q31_t product = (q31_t)handle->complexOutput[2*i] * 
    //                    (q31_t)hammingWindow[i];
    //     handle->complexOutput[2*i] = (q15_t)(product >> 15);  // Q15乘法结果处理
    // }
    
    //3. 执行FFT计算
    CFFT_Compute(handle);
}

// ADC数据处理函数
void CFFT_ProcessADCData_hamming(CFFT_Handle *handle, uint16_t *adcData, uint32_t dataSize) {
    // 0. 确保汉明窗已初始化（只需一次）
    static uint8_t windowInitialized = 0;
    if (!windowInitialized) {
        CFFT_InitHammingWindow();
        windowInitialized = 1;
    }

    // 1. 将12位ADC数据转换为Q15格式
    for (uint32_t i = 0; i < dataSize && i < CFFT_NUM_SAMPLES; i++) {
        // 转换12位ADC到Q15格式：减去直流偏移并缩放
        handle->input[i] = (q15_t)((adcData[i] - 2048) * 8); // -2048~2047 -> -16384~16376
        
        // 构建复数输入（虚部设为0）
        handle->complexOutput[2*i] = handle->input[i]; // 实部
        handle->complexOutput[2*i+1] = 0;              // 虚部
    }
    
    // 2. 填充剩余数据（如果输入不足）
    for (uint32_t i = dataSize; i < CFFT_NUM_SAMPLES; i++) {
        handle->complexOutput[2*i] = 0;
        handle->complexOutput[2*i+1] = 0;

    }
    // // >>>>>>>> 新增：加汉明窗操作（核心位置）<<<<<<<<
    for (int i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 仅对实部加窗（虚部保持为0）
        q31_t product = (q31_t)handle->complexOutput[2*i] * 
                       (q31_t)hammingWindow[i];
        handle->complexOutput[2*i] = (q15_t)(product >> 15);  // Q15乘法结果处理
    }
    
    //3. 执行FFT计算
    CFFT_Compute(handle);
}

void CFFT_f32(float32_t *InputSample, float32_t *ComplexBuffer, float32_t *Mag_Buffer)
{
    // 0. 确保汉明窗已初始化（只需一次）
    static uint8_t windowInitialized = 0;
    if (!windowInitialized) {
        CFFT_InitHammingWindow();
        windowInitialized = 1;
    }
    // 1. 检查参数有效性
    if (InputSample == NULL || ComplexBuffer == NULL || Mag_Buffer == NULL) {
        return; // 错误处理：参数为空
    }

    // 2. 将实部数据复制到复数缓冲区（虚部置0）
    for (uint32_t i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 关键修改：输入信号乘汉明窗系数
        ComplexBuffer[2 * i]  = InputSample[i] * hammingWindow[i]; // 加窗实部
        ComplexBuffer[2 * i + 1] = 0.0f;                            // 虚部置零
    }

    // 3. 执行前向FFT（使用预定义的FFT实例）
    arm_cfft_f32(&arm_cfft_sR_f32_len2048, ComplexBuffer, 0, 1);

    // 4. 计算幅度谱
    arm_cmplx_mag_f32(ComplexBuffer, Mag_Buffer, CFFT_NUM_SAMPLES);

    // 5. 幅值校准（直流分量/交流分量分别缩放）
    Mag_Buffer[0] /= CFFT_NUM_SAMPLES;          // 直流分量除以N
    for (uint32_t i = 1; i < CFFT_NUM_SAMPLES; i++) {
        Mag_Buffer[i] /= (CFFT_NUM_SAMPLES / 2); // 交流分量除以N/2
    }
}

void CFFT_f32_Only_Complex(float32_t *InputSample, float32_t *ComplexBuffer)
{
    // 0. 确保汉明窗已初始化（只需一次）
    static uint8_t windowInitialized = 0;
    if (!windowInitialized) {
        CFFT_InitHammingWindow();
        windowInitialized = 1;
    }
    // 1. 检查参数有效性
    if (InputSample == NULL || ComplexBuffer == NULL) {
        return; // 错误处理：参数为空
    }

    // 2. 将实部数据复制到复数缓冲区（虚部置0）
    for (uint32_t i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 关键修改：输入信号乘汉明窗系数
        ComplexBuffer[2 * i]  = InputSample[i] * hammingWindow[i]; // 加窗实部
        ComplexBuffer[2 * i + 1] = 0.0f;                            // 虚部置零
    }

    // 3. 执行前向FFT（使用预定义的FFT实例）
    arm_cfft_f32(&arm_cfft_sR_f32_len2048, ComplexBuffer, 0, 1);
}

void CIFFT_f32(float32_t *InputComplexSample, float32_t *Time_Buffer)
{
    // 2. 执行复数IFFT（原地计算，直接修改输入数组）
    arm_cfft_f32(&arm_cfft_sR_f32_len2048, InputComplexSample, 1, 1);

    // 3. 提取实部并缩放输出
    for (uint32_t i = 0; i < CFFT_NUM_SAMPLES; i++) {
        // 直接提取实部（偶索引）并缩放
        Time_Buffer[i] = InputComplexSample[2 * i] / CFFT_NUM_SAMPLES / hammingWindow[i] * 2048;
        
        // 可选：添加虚部验证代码（调试用）
        // float imag = InputComplexSample[2 * i + 1];
        // if (fabsf(imag) > 1e-5) {/* 警告：非零虚部 */}
    }
}

//直接生成波形
void generate_waveform_directly(CFFT_Handle* handle,q15_t *Complex_Buffer, q15_t target_peak_amplitude, q63_t *max_value)
{
    // CFFT_NUM_SAMPLES 在 CFFT.h 中定义为 1024 [cite: 1]
    const int N = CFFT_NUM_SAMPLES; 
    q63_t temp_buffer[N];
    q63_t max_raw_val = 0; // 用于记录第一遍扫描时的最大绝对值
    // --- 第一遍扫描：合成原始波形并找到最大值 ---
    for (int n = 0; n < N; n++) {
        q63_t acc = 0; // 使用64位累加器防止溢出

        // 从k=1开始循环到奈奎斯特频率，k=0是直流分量，暂不处理
        for (int k = 1; k < N / 2; k++) {
            q15_t real_k = Complex_Buffer[2 * k];
            q15_t imag_k = Complex_Buffer[2 * k + 1];

            // 如果这个频率点没有值，就跳过，以提高效率
            if (real_k == 0 && imag_k == 0) {
                continue;
            }

            // 计算相位角度，使用Q15格式的三角函数
            // arm_sin_q15 和 arm_cos_q15 的输入是 0-32767 代表 0-2PI
            q15_t angle = (q15_t)(((q31_t)n * k * 32768) / (N));
            q15_t cos_val = arm_cos_q15(angle);
            q15_t sin_val = arm_sin_q15(angle);
            
            // 根据反变换公式累加贡献：y[n] = Σ (Real[k]*cos - Imag[k]*sin)
            // 乘以2是因为我们只循环到N/2，需要补偿共轭对称的另一半
            acc += 2 * ((q63_t)real_k * cos_val - (q63_t)imag_k * sin_val);
        }
        
        // 存储未缩放的原始值
        temp_buffer[n] = acc;

        // 记录最大绝对值
        q63_t current_abs_val = (acc < 0) ? -acc : acc;
        if (current_abs_val > max_raw_val && n > N/8) {
            max_raw_val = current_abs_val;
        }
        *max_value = max_raw_val;
    }

    // --- 第二遍扫描：归一化和缩放 ---
    
    // 防止除零错误
    if (max_raw_val == 0) {
        // 如果所有值都是0，直接清空输出缓冲区
        return;
    }
    
    // 缩放原始波形到目标峰值，并存入最终的q15_t输出缓冲区
    for (int n = 0; n < N; n++) {
        // 计算缩放后的值
        q63_t scaled_val = (temp_buffer[n] * target_peak_amplitude) / max_raw_val;
        // 饱和处理，防止意外溢出
        if (scaled_val > 32767) scaled_val = 32767;
        if (scaled_val < -32768) scaled_val = -32768;
        handle->input[n] = (q15_t)(scaled_val);
    }
    // __BKPT();
}