
#include "ti_msp_dl_config.h"
#include "arm_math.h"
#include "math.h"
#include "CFFT.h"
#include "LCD.h"
#include "dsp_9910.h"

#define FFT_SIZE 1024
bool DMA1_flag = false;
bool DMA2_flag = false;
bool UART_flag = false;
bool Reverse_flag = true;
uint8_t Res;
char str[30];

#define SAMPLE_RATE     1000000.0f   // ADC 采样率
#define TWO_PI          6.283185307f
#define MIN_FREQ        100.0f
#define MAX_FREQ        500000.0f
#define NOISE_FLOOR     0.1f
#define SAMPLE_INTERVAL_ns 1000
#define CH1          0
#define CH2          1
#define DEBUG        0


typedef enum {
    State_Band_Pass,
    State_Narrow_Band_Pass, // 新增窄带状态
    State_Band_Stop,
    State_Low_Pass,
    State_High_Pass,
    State_All_Pass,         // 全通
    State_Fault             // 异常
} CircuitState;

q63_t max_value_after_ifft;



static q15_t smoothed[SWEEP_POINTS];
static q15_t Buffer_Muti_Result[FFT_SIZE*2];
static q15_t Buffer_sweep_Angle[SWEEP_POINTS];
static q15_t Buffer_sweep_Mag[SWEEP_POINTS];
static q15_t Buffer_sweep_real[SWEEP_POINTS];
static q15_t Buffer_sweep_imag[SWEEP_POINTS];
static q15_t Buffer_sweep[SWEEP_POINTS*2];
uint16_t gADCSamples_x[FFT_SIZE];  //PB27
uint16_t gADCSamples_y[FFT_SIZE];  //PA25
// float32_t gADC_y[FFT_SIZE];
// float32_t gADC_x[FFT_SIZE];
// float32_t Complex_Buffer[2 * FFT_SIZE];
// float32_t Mag_Buffer[FFT_SIZE];
static q15_t inputBuffer[FFT_SIZE];//输入数据，即采集的ADC
static q15_t complexBuffer[2 * FFT_SIZE]; // 复数缓冲区
static q15_t magnitudeBuffer[FFT_SIZE];   // 幅度结果
// Q15格式的Savitzky-Golay系数（整数缩放版）
const int16_t sg_coeffs_q15[5] = {
    (int16_t)(-3.0/35 * 32768),  // -2818 (Q15)
    (int16_t)(12.0/35 * 32768),  // 11274 (Q15)
    (int16_t)(17.0/35 * 32768),  // 15982 (Q15)
    (int16_t)(12.0/35 * 32768),  // 11274 (Q15)
    (int16_t)(-3.0/35 * 32768)   // -2818 (Q15)
};

CFFT_Handle fftHandle; //cfft结构体
// float32_t Time_Buffer[FFT_SIZE];
// CFFT_Handle fftHandle; //cfft结构体
// arm_cfft_instance_q15 S;
char str[30];
uint8_t CH_flag = 1;
uint8_t Current_State;
uint32_t Current_freq = 100000;
uint32_t Current_Amp_mV = 1500;


//DEBUG参数
float Debug_Ratio_for_HighFreq = 1.66;   //新：调节高频输出
float Debug_Ratio_for_Adjust_Vpp = 1.37;
float Debug_Ratio_for_OutSignal = 0.83;          //调节输出信号幅值
uint16_t DAC_Set_Sweep = 900;       //扫频时的DAC幅值  
float Debug_Index_Ratio = 0.992;
uint16_t Debug_DAC_SET_LF = 1075;

//函数声明
void Set_PWM_DutyCycle_and_Freq(uint32_t Freq, float duty, uint8_t channel);
void DAC_Set(uint16_t v_value_mv);
float Find_DC_OffSet_y(void);
float Find_DC_OffSet_x(void);
float calculate_period_in_samples(uint16_t* samples, uint16_t size, float dc_offset);
void Send_SimFreq_Sin(void);
float Cal_HsAmp(uint32_t freq);
float Find_Vpp(float32_t *Input_sample, uint16_t size);
uint16_t Find_Vpp_Int(uint16_t *Input_sample, uint16_t size);
void getADCsamples(void);
void Sweep_Freq_y(q15_t *Buffer_sweep);


void Judge_Circuit_State(float *Buffer_sweep);
void Freq_Multip(float32_t *Complex_Buffer, float *Sweep_Complex, float *Result_Buffer);
void DDS_Convert_With_Outside(float freq, uint Amp_mv);
void Optimized_Judge_Circuit_State(q15_t *Buffer_sweep);
void Amp_Adjust_FeedBack(uint16_t Amp_mv_set, float freq, uint16_t Current_Amp);
//基础（4）
void Output_1to2V_Sin(float freq, uint16_t Amp_mV);
void Freq_Multip_q15(q15_t *Complex_Buffer, q15_t *Sweep_Complex, q15_t *Result_Buffer, uint32_t size);
float Find_DC_OffSet_Input(uint16_t *Sample);
void Output_After_Learning(q15_t *Complex_Buffer,float Amp);
void change_ADCsample_rate(uint32_t sample_rate_hz);
void savgol_smooth_q15(int16_t* input, int16_t* output);
float Calculate_Output_Vpp_From_Spectrum(q15_t *Muti_Result_Complex_Buffer, uint32_t size);
void Amp_Adjust_FeedBack_Random(uint16_t Amp_mv_set, uint16_t DAC_gain);
void Amp_Adjust_FeedBack_Random_OutSignal(uint16_t Amp_mv_set, uint16_t DAC_gain);
uint32_t Get_Freq_From_FreqBase(void);
float Find_DC_OffSet_Input_L(uint16_t *Sample);
float Find_DC_OffSet_Input_M(uint16_t *Sample);
void convertTo1000kSpec(q15_t* complexBuffer, uint32_t fftSize);//频谱转换


int main(void)
{
    SYSCFG_DL_init();
    AD9910_Init();
    // arm_cfft_init_q15(&S, FFT_SIZE);
    // 手动关联内存（替代动态分配）
    DAC_Set(1000);
     //配置DMA
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) 0x40556280);//DMA取MEM0的数据
    //DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) &gADCSamples_x[0]);//DMA存数据到数组
    DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t) 0x40558280);//DMA取MEM0的数据
    //DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t) &gADCSamples_y[0]);//DMA存数据到数组
    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC12_1_INST_INT_IRQN);
    DL_TimerA_startCounter(PWM_0_INST);

     //串口设置
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    // 首次使用FFT估计信号频率
    // 手动关联内存（替代动态分配）
    fftHandle.input = inputBuffer;//用来处理输入信号
    fftHandle.complexOutput = complexBuffer;//输出缓冲区
    fftHandle.magnitude = magnitudeBuffer;//输出幅度谱

    while(!DEBUG)
    {
        while(UART_flag == false);
        Res=DL_UART_Main_receiveData(UART_0_INST);
        UART_flag = false;
        if(Res == 0xA1)  //+100Hz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq + 100) < 1100000)
            {
                Current_freq += 100;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
            }
            else if(CH_flag == CH2 && (Current_freq + 100) <= 3000)
            {
                change_ADCsample_rate(10000);
                Current_freq += 100;
                Output_1to2V_Sin((float)Current_freq,Current_Amp_mV);
                change_ADCsample_rate(1000000);
            }
            sprintf(str,"%d Hz", Current_freq);
            sendStr_New(str,1,6);
            
        }
        else if(Res == 0xA2) //-100Hz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq - 100) >= 100)
            {
                Current_freq -= 100;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
                   
            }
            else if(CH_flag == CH2 && (Current_freq - 100) >= 100)
            {
                change_ADCsample_rate(10000);
                Current_freq -= 100;
                Output_1to2V_Sin((float)Current_freq,Current_Amp_mV);
                change_ADCsample_rate(1000000);
            }
            sprintf(str,"%d Hz", Current_freq);
            sendStr_New(str,1,6);
        }
        else if(Res == 0x21) //+50kHz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq + 50000) <= 1100000)
            {
                Current_freq += 50000;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
                sprintf(str,"%d Hz", Current_freq);
                sendStr_New(str,1,6);
            }
        }
        else if(Res == 0x22) //-50kHz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq - 50000) >= 100)
            {
                Current_freq -= 50000;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
                sprintf(str,"%d Hz", Current_freq);
                sendStr_New(str,1,6);
            }
        }
        else if(Res == 0x23) //+2kHz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq + 2000) <= 1100000)
            {
                Current_freq += 2000;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
                sprintf(str,"%d Hz", Current_freq);
                sendStr_New(str,1,6);
            }
        }
        else if(Res == 0x24) //-2kHz
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH1 && (Current_freq - 2000) >= 100)
            {
                Current_freq -= 2000;
                DDS_Convert_With_Outside(Current_freq, 1100);
                Amp_Adjust_FeedBack_Random(3600,1000);
                sprintf(str,"%s", "> 3Vpp");
                sendStr_New(str,1,12);
                sprintf(str,"%d Hz", Current_freq);
                sendStr_New(str,1,6);
            }
        }
        else if(Res == 0xA4) //+100mV
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH2 && (Current_Amp_mV + 100) <= 2000)
            {
                change_ADCsample_rate(10000);
                Current_Amp_mV += 100;
                Output_1to2V_Sin((float)Current_freq,Current_Amp_mV);
                sprintf(str,"%d mV", Current_Amp_mV);
                sendStr_New(str,1,12);
                change_ADCsample_rate(1000000);
            }
        }
        else if(Res == 0xA5) //-100mV
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            if(CH_flag == CH2 && (Current_Amp_mV - 100) >= 1000)
            {
                change_ADCsample_rate(10000);
                Current_Amp_mV -= 100;
                Output_1to2V_Sin((float)Current_freq,Current_Amp_mV);
                sprintf(str,"%d mV", Current_Amp_mV);
                sendStr_New(str,1,12);
                change_ADCsample_rate(1000000);
            }
        }
        else if(Res == 0xA3) //高频输出
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            CH_flag = CH1;
            Current_Amp_mV = 1100;
            Current_freq = 1000000;
            DDS_Convert_With_Outside(Current_freq, 1100);
            Amp_Adjust_FeedBack_Random(3600,1000);
            sprintf(str,"%s", "> 3Vpp");
            sendStr_New(str,1,12);
            sprintf(str,"%d Hz", Current_freq);
            sendStr_New(str,1,6);
        }
        else if(Res == 0xA6) //控压输出
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            CH_flag = CH2;
            Current_freq = 1500;
            Current_Amp_mV = 1500;
            Output_1to2V_Sin((float)Current_freq,Current_Amp_mV);
            sprintf(str,"%d mV", Current_Amp_mV);
            sendStr_New(str,1,12);
            sprintf(str,"%d Hz", Current_freq);
            sendStr_New(str,1,6);
        }
        else if(Res == 0x1A) //学习建模
        {
            DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            sprintf(str,"%s", "Learning");
            sendStr_New(str,2,6);
            
            sprintf(str,"%s", "Default");
            sendStr_New(str,2,9);
            Sweep_Freq_y(Buffer_sweep);
            sprintf(str,"%s", "Over");
            sendStr_New(str,2,6);
            if(Current_State == State_All_Pass) sprintf(str,"%s", "All_Pass");
            else if(Current_State == State_Band_Pass) sprintf(str,"%s", "Band_Pass");
            else if(Current_State == State_High_Pass) sprintf(str,"%s", "High_Pass");
            else if(Current_State == State_Low_Pass) sprintf(str,"%s", "Low_Pass");
            else if(Current_State == State_Narrow_Band_Pass) sprintf(str,"%s", "Band_Pass");
            else if(Current_State == State_Band_Stop) sprintf(str,"%s", "Band_Stop");
            else if(Current_State == State_Fault) sprintf(str,"%s", "Fault");
            sendStr_New(str,2,9);
        }
        else if(Res == 0x1B) //推理输出
        {
            DL_GPIO_setPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            sprintf(str,"%s", "Outputing");
            sendStr_New(str,2,6);
            Output_After_Learning(complexBuffer,3.3);
            //同频显示收到串口消息需要有break
            sprintf(str,"%s", "Over");
            sendStr_New(str,2,6);
        }

        //DEBUG
        else if(Res == 0x91)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_OutSignal = Debug_Ratio_for_OutSignal - 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_OutSignal);
            sendStr_New(str,3,6);
        }
        else if(Res == 0x92)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_OutSignal = Debug_Ratio_for_OutSignal + 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_OutSignal);
            sendStr_New(str,3,6);
        }
        else if(Res == 0x93)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_HighFreq = Debug_Ratio_for_HighFreq - 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_HighFreq);
            sendStr_New(str,3,9);
        }
        else if(Res == 0x94)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_HighFreq = Debug_Ratio_for_HighFreq + 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_HighFreq);
            sendStr_New(str,3,9);
        }
        else if(Res == 0x95)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            DAC_Set_Sweep = DAC_Set_Sweep - 1;
            sprintf(str,"%d", DAC_Set_Sweep);
            sendStr_New(str,3,18);
        }
        else if(Res == 0x96)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            DAC_Set_Sweep = DAC_Set_Sweep + 1;
            sprintf(str,"%d", DAC_Set_Sweep);
            sendStr_New(str,3,18);
        }
        else if(Res == 0x97)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_Adjust_Vpp = Debug_Ratio_for_Adjust_Vpp - 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_Adjust_Vpp);
            sendStr_New(str,3,25);
        }
        else if(Res == 0x98)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Ratio_for_Adjust_Vpp = Debug_Ratio_for_Adjust_Vpp + 0.01;
            sprintf(str,"%.2f", Debug_Ratio_for_Adjust_Vpp);
            sendStr_New(str,3,25);
        }
        else if(Res == 0x99)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Reverse_flag = !Reverse_flag;
        }
        else if(Res == 0x9A)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Index_Ratio = Debug_Index_Ratio - 0.001;
            sprintf(str,"%.3f", Debug_Index_Ratio);
            sendStr_New(str,3,33);
        }
        else if(Res == 0x9B)
        {
            //DL_GPIO_clearPins(GPIO_KEY_PORT, GPIO_KEY_PIN_A1_PIN);
            Debug_Index_Ratio = Debug_Index_Ratio + 0.001;
            sprintf(str,"%.3f", Debug_Index_Ratio);
            sendStr_New(str,3,33);
        }
        
        
    }
    while (1) 
    {
        
        //DAC_Set(800);
        DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&gADCSamples_x[0]);  //ADC0
        DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, FFT_SIZE);
        DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
        DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&gADCSamples_y[0]);  //ADC1
        DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, FFT_SIZE);
        DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
        DL_TimerG_startCounter(TIMER_0_INST);
        while(DMA1_flag == false || DMA2_flag == false);
        DMA1_flag = false;
        DMA2_flag = false;

        // Sweep_Freq_y(Buffer_sweep);
        // adc_to_cfft_q15(gADCSamples_y,fft_out,FFT_SIZE);
        // __BKPT();
        // adc_cifft_q15(fft_out, time_out, FFT_SIZE);
        // __BKPT();
        // for(uint16_t i = 0;i<1000;i++)
        // {
        //     Freq_convert(200 * i);
        //     delay_cycles(4000000);
        // }
        for(uint32_t i = 1000;i<=2000;i += 100)
        {
            float freq = 1000.0f;
            float HS = Cal_HsAmp(freq);
            uint Amp = (uint)(1200.0 / HS);
            
            // DDS_Convert_With_Outside(freq,Amp);
            // Amp_Adjust_FeedBack(Amp, freq, Amp);
            // __BKPT();
            DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&gADCSamples_x[0]);  //ADC0
            DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, FFT_SIZE);
            DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
            DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&gADCSamples_y[0]);  //ADC1
            DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, FFT_SIZE);
            DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
            DL_TimerG_startCounter(TIMER_0_INST);
            while(DMA1_flag == false || DMA2_flag == false);
            DMA1_flag = false;
            DMA2_flag = false;
            CFFT_ProcessADCData(&fftHandle, gADCSamples_x, FFT_SIZE);
            __BKPT();
            //Sweep_Freq_y(Buffer_sweep);
            //Sweep_Freq_LF(Buffer_sweep_LF);
            //savgol_smooth_q15(Buffer_sweep_real, smoothed);
        
            
            __BKPT();

            // getADCsamples();
            // CFFT_ProcessADCData(&fftHandle, gADCSamples_x, FFT_SIZE);
            while(1)
            {
                Output_After_Learning(complexBuffer,3.3);
            }
            // Freq_Multip_q15(complexBuffer,Buffer_sweep,Buffer_Muti_Result,FFT_SIZE);
        }
        // for(uint16_t i = 0; i<FFT_SIZE;i++)
        // {
        //     gADC_y[i] = gADCSamples_y[i] * 1.0 / 4095 * 3.3;
        // }
        // CFFT_f32(gADC_y,Complex_Buffer,Mag_Buffer);

        // //__BKPT();
        // CIFFT_f32(Complex_Buffer, Time_Buffer); //执行后Complex_buffer会变
        

        float dc_offset_y = Find_DC_OffSet_y();
        float Sample_dots = calculate_period_in_samples(gADCSamples_y, FFT_SIZE, dc_offset_y);
        float freq = (1000000000.0 / (Sample_dots * 1220));
        
        //OutputArbitraryWaveform_NN(freq, gADC_y, (int)Sample_dots);

        __BKPT();
        
        
        
    }
}

void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_RX://DMA_DONE
            UART_flag = true;//修改标志位
            
            break;
        default:
            break;
    }
}

void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_DMA_DONE:
            DMA1_flag = true;
            break;
        default:
            break;
    }
}

void ADC12_1_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_1_INST)) {
        case DL_ADC12_IIDX_DMA_DONE:
            DMA2_flag = true;
            break;
        default:
            break;
    }
}

void Set_PWM_DutyCycle_and_Freq(uint32_t Freq, float duty, uint8_t channel)
{
    uint32_t Period = PWM_0_INST_CLK_FREQ / Freq;
    uint32_t CompareValue = Period - Period * duty;
    DL_Timer_setLoadValue(PWM_0_INST, Period);
    if(channel == 0)
        DL_Timer_setCaptureCompareValue(PWM_0_INST, CompareValue, DL_TIMER_CC_0_INDEX);
    else if(channel == 1)
        DL_Timer_setCaptureCompareValue(PWM_0_INST, CompareValue, DL_TIMER_CC_1_INDEX);

}



void DAC_Set(uint16_t v_value_mv){
    uint32_t DAC_value;
    /* Set output voltage:
     *  DAC value (12-bits) = DesiredOutputVoltage x 4095
     *                          -----------------------
     *                              ReferenceVoltage
     */
    DAC_value = (v_value_mv * 4095) / 3300;
    DL_DAC12_output12(DAC0, DAC_value);
    DL_DAC12_enable(DAC0);
}

float Find_DC_OffSet_y(void) 
{
    uint16_t min_val = UINT16_MAX; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < FFT_SIZE; i++) {
        uint16_t sample = gADCSamples_y[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return 1.0 * (max_val + min_val) / 2; // 
}

float Find_Vpp(float32_t *Input_sample, uint16_t size) 
{
    float32_t min_val = 10000.0; // 初始化为最大可能值
    float32_t max_val = 0.0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < size; i++) {
        float32_t sample = Input_sample[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return (max_val - min_val); // 
}

uint16_t Find_Vpp_Int(uint16_t *Input_sample, uint16_t size) 
{
    uint16_t min_val = 10000; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < size; i++) {
        uint16_t sample = Input_sample[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return (max_val - min_val); // 
}

float Find_DC_OffSet_x(void) 
{
    uint16_t min_val = UINT16_MAX; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < FFT_SIZE; i++) {
        uint16_t sample = gADCSamples_x[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return 1.0 * (max_val + min_val) / 2; // 
}

/**
 * @brief 计算信号的一个完整周期所包含的采样点数
 * @param samples ADC采样数据数组
 * @param size 数组大小
 * @param dc_offset 直流偏置
 * @return 返回周期的采样点数。如果找不到一个完整周期，返回-1.0
 */
float calculate_period_in_samples(uint16_t* samples, uint16_t size, float dc_offset) 
{
    
    float first_crossing = -1.0f;
    float second_crossing = -1.0f;

    for (uint16_t i = 1; i < size; i++) {
        if (samples[i-1] < dc_offset && samples[i] >= dc_offset) {
            float val_prev = (float)samples[i-1];
            float val_curr = (float)samples[i];
            float fraction = (dc_offset - val_prev) / (val_curr - val_prev);
            float current_crossing = (float)(i - 1) + fraction;

            if (first_crossing < 0) {
                // 这是找到的第一个过零点
                first_crossing = current_crossing;
            } else {
                // 这是第二个，我们找到了一个完整周期
                second_crossing = current_crossing;
                return second_crossing - first_crossing; // 返回两个过零点之间的采样点数差
            }
        }
    }
    return -1.0f; // 采样数据中不足一个完整周期
}

void Send_SimFreq_Sin(void)
{
    float Offset = Find_DC_OffSet_y();
    float T = calculate_period_in_samples(gADCSamples_y,FFT_SIZE,Offset);
    T = T * 977;
    if(T > 0)
    {
        float freq = 1000000000.0f / T;
        Freq_convert(freq );
    }
}

float Cal_HsAmp(uint32_t freq)
{
    float omiga = TWO_PI * (float)freq;
    float Hs_ABS = (float)(5.0 / sqrt(
    (1.0 - (double)omiga*(double)omiga/100000000.0) * (1.0 - (double)omiga*(double)omiga/100000000.0) + 
    (3.0 * (double)omiga/10000.0) * (3.0 * (double)omiga/10000.0)
));
    return Hs_ABS;
}

void Sweep_Freq_y(q15_t *Buffer_sweep)
{
    q63_t real_num, imag_num, denom;
    q31_t H_real, H_imag;
    DAC_Set(900);
    //__BKPT();
    // 循环遍历所有扫频点
    for (uint i = 0; i < SWEEP_POINTS; i++)
    {
        float current_freq = START_FREQ + (i * FREQ_STEP);

        // 安全检查
        if (current_freq > END_FREQ)
        {
            current_freq = END_FREQ;
        }
        // Freq_convert(current_freq); 
        DDS_Convert_With_Outside(current_freq, 2000);
        delay_cycles(32000); 

        getADCsamples();
        
        
        CFFT_ProcessADCData(&fftHandle, gADCSamples_x, FFT_SIZE);
        uint32_t index = fftHandle.maxIndex + 3;
        //uint32_t index = round((double)(current_freq * FFT_SIZE / SAMPLE_RATE));
        // 将测得的增益值存放到数组的相应位置
        q15_t X_real = complexBuffer[index*2];
        q15_t X_imag = complexBuffer[index*2+1];

        CFFT_ProcessADCData(&fftHandle, gADCSamples_y, FFT_SIZE);
        q15_t Y_real = complexBuffer[2*index];
        q15_t Y_imag = complexBuffer[2*index+1];
        // 计算频率响应 H(s) = Y/X (复数除法)
        // 转换为Q31精度防止溢出
        q31_t X_real_q31 = (q31_t)X_real << 16;
        q31_t X_imag_q31 = (q31_t)X_imag << 16;
        q31_t Y_real_q31 = (q31_t)Y_real << 16;
        q31_t Y_imag_q31 = (q31_t)Y_imag << 16;
        //if(i==320) __BKPT();
        // if(i==200) __BKPT();
        // if(i==300) __BKPT();
        // 计算分母 |X|² = X_real² + X_imag²
        denom = (q63_t)X_real_q31 * X_real_q31 + 
                (q63_t)X_imag_q31 * X_imag_q31;
        if (denom == 0) denom = 1; 

        // 计算分子 Y·X共轭
        real_num = (q63_t)Y_real_q31 * X_real_q31 + 
                  (q63_t)Y_imag_q31 * X_imag_q31;
        imag_num = (q63_t)Y_imag_q31 * X_real_q31 - 
                  (q63_t)Y_real_q31 * X_imag_q31;

        // 执行除法 (Y·X*) / |X|²
        H_real = (q31_t)(real_num / (denom >> 31));
        H_imag = (q31_t)(imag_num / (denom >> 31));

        Buffer_sweep[2*i]   = (q15_t)(H_real >> 16);  // 实部存入偶数索引
        Buffer_sweep_real[i] = Buffer_sweep[2*i];
        Buffer_sweep[2*i+1] = (q15_t)(H_imag >> 16);  // 虚部存入奇数索引
        Buffer_sweep_imag[i] = Buffer_sweep[2*i+1];
        Buffer_sweep_Mag[i] = (q15_t)sqrt(Buffer_sweep_real[i]*Buffer_sweep_real[i]+Buffer_sweep_imag[i]*Buffer_sweep_imag[i]);

         // 新增：计算相位谱并存储到 Buffer_sweep_Angle
        // 步骤1：Q31 → 浮点数（范围[-1, 1]）
        //float real_f = (float)H_real / (1LL << 31);  // 2^31
        //float imag_f = (float)H_imag / (1LL << 31);
        // 步骤2：计算相位角（弧度，范围[-π, π]）
       // float angle_rad = atan2f(imag_f, real_f);
        // 步骤3：归一化到Q15范围 [-32768, 32767] 对应 [-π, π]
        //q15_t angle_q15 = (q15_t)(angle_rad / M_PI * 32768);
        // 存储相位谱结果
       // Buffer_sweep_Angle[i] = angle_q15;
    }
    Optimized_Judge_Circuit_State(Buffer_sweep_Mag);
}


void getADCsamples_x(void)
{
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&gADCSamples_x[0]);  //ADC0
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, FFT_SIZE);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
   
    DL_TimerG_startCounter(TIMER_0_INST);
    while(DMA2_flag == false);
    DMA2_flag = false;
}

void getADCsamples(void)
{
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&gADCSamples_x[0]);  //ADC0
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, FFT_SIZE);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&gADCSamples_y[0]);  //ADC1
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, FFT_SIZE);
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
    DL_TimerG_startCounter(TIMER_0_INST);
    while(DMA1_flag == false || DMA2_flag == false);
    DMA1_flag = false;
    DMA2_flag = false;
}

void Judge_Circuit_State(float *Buffer_sweep)
{
    // 1. 寻找最大增益值作为参考
    float max_gain = 0.0f;
    for (uint i = 0; i < SWEEP_POINTS; i++) {
        if (Buffer_sweep[2*i] > max_gain) {
            max_gain = Buffer_sweep[2*i];
        }
    }

    // 2. 计算各频段的平均增益
    const float threshold = max_gain * 0.5f;  // 半功率点阈值
    uint low_band_points = SWEEP_POINTS / 4;
    uint high_band_points = SWEEP_POINTS / 4;
    uint mid_band_points = SWEEP_POINTS / 2;
    
    float low_band_avg = 0.0f;
    float high_band_avg = 0.0f;
    float mid_band_avg = 0.0f;
    
    // 计算低频段（前1/4）
    for (uint i = 0; i < low_band_points; i++) {
        low_band_avg += Buffer_sweep[i];
    }
    low_band_avg /= low_band_points;
    
    // 计算高频段（后1/4）
    for (uint i = SWEEP_POINTS - high_band_points; i < SWEEP_POINTS; i++) {
        high_band_avg += Buffer_sweep[i];
    }
    high_band_avg /= high_band_points;
    
    // 计算中频段（中间1/2）
    uint mid_start = (SWEEP_POINTS - mid_band_points) / 2;
    for (uint i = mid_start; i < mid_start + mid_band_points; i++) {
        mid_band_avg += Buffer_sweep[i];
    }
    mid_band_avg /= mid_band_points;

    // 3. 根据增益分布判断电路类型
    if ((low_band_avg < threshold) && 
        (high_band_avg < threshold) && 
        (mid_band_avg > threshold)) {
        Current_State = State_Band_Pass;  // 带通特性
    } 
    else if ((low_band_avg > threshold) && 
             (high_band_avg > threshold) && 
             (mid_band_avg < threshold)) {
        Current_State = State_Band_Stop;  // 带阻特性
    } 
    else if ((low_band_avg > threshold) && 
             (high_band_avg < threshold)) {
        Current_State = State_Low_Pass;   // 低通特性
    } 
    else if ((low_band_avg < threshold) && 
             (high_band_avg > threshold)) {
        Current_State = State_High_Pass;  // 高通特性
    } 
    else {
        Current_State = State_Band_Pass;  // 默认设为带通
    }
}

void Optimized_Judge_Circuit_State(q15_t *Buffer_sweep) {
    // 1. 动态计算最大/最小幅度值（幅度谱处理）
    q15_t max_amp = -32767, min_amp = 32767;
    for (uint i = 0; i < SWEEP_POINTS - 32; i++) {
        q15_t amplitude = Buffer_sweep[i]; // 直接获取幅度值
        if (amplitude > max_amp) max_amp = amplitude;
        if (amplitude < min_amp) min_amp = amplitude;
    }
    
    // 噪声门限检测（使用绝对值）
    if (max_amp < NOISE_FLOOR) {
        Current_State = State_Fault;
        return;
    }

    // 2. 定位-3dB截止点（基于幅度谱）
    uint lower_cutoff = 0, upper_cutoff = SWEEP_POINTS - 1;
    const float THRESHOLD_RATIO = 0.707f; // 1/sqrt(2)
    const q15_t AMP_THRESHOLD = (q15_t)(max_amp * THRESHOLD_RATIO);
    
    // 正向扫描寻找下截止点
    for (uint i = 3; i < SWEEP_POINTS; i++) {
        if (Buffer_sweep[i] >= AMP_THRESHOLD) {
            lower_cutoff = i;
            break;
        }
    }
    
    // 反向扫描寻找上截止点
    for (uint i = SWEEP_POINTS - 32; i > 1; i--) {
        if (Buffer_sweep[i] >= AMP_THRESHOLD) {
            upper_cutoff = i;
            break;
        }
    }

    // 3. 计算频段能量占比（幅度谱能量计算）
    float total_energy = 0.0f;
    float passband_energy = 0.0f;
    
    // 幅度谱能量计算：E = Σ|A[i]|^2
    for (uint i = 3; i < SWEEP_POINTS-10; i++) {
        // 转换为能量值（避免负数平方问题）
        float energy = (float)((uint)Buffer_sweep[i] * (uint)Buffer_sweep[i]) / 32767.0f;
        total_energy += energy;
        
        if (i >= lower_cutoff && i <= upper_cutoff) {
            passband_energy += energy;
        }
    }
    
    // 计算阻带能量占比
    float stopband_ratio = (total_energy - passband_energy) / total_energy;

    // 4. 状态决策树（基于幅度谱特征）
    const float BANDWIDTH_RATIO = (float)(upper_cutoff - lower_cutoff) / SWEEP_POINTS;
    
    
    if (lower_cutoff > 3 && upper_cutoff < SWEEP_POINTS - 32) {
        Current_State = (BANDWIDTH_RATIO < 0.1f) 
                          ? State_Narrow_Band_Pass : State_Band_Pass;
    } else if (lower_cutoff == 3 && upper_cutoff < SWEEP_POINTS - 32) {
        Current_State = State_Low_Pass;
    } else if(lower_cutoff > 3 && upper_cutoff == SWEEP_POINTS - 32)
    {
        Current_State = State_High_Pass;
    }
    else if (lower_cutoff == 3 && upper_cutoff == SWEEP_POINTS - 32) {
        Current_State = State_Band_Stop;
    }
}

void Freq_Multip(float32_t *Complex_Buffer, float *Sweep_Complex, float *Result_Buffer)
{
    for (uint16_t k = 0; k < FFT_SIZE; k++) 
    {
        float real1 = Complex_Buffer[2*k];     // 信号谱实部
        float imag1 = Complex_Buffer[2*k+1];   // 信号谱虚部
        float real2 = Sweep_Complex[2*k];      // 扫频谱实部（幅度）
        float imag2 = Sweep_Complex[2*k+1];    // 扫频谱虚部

        // 复数乘法: (a+bi)*(c+di) = (ac-bd) + (ad+bc)i
        Result_Buffer[2*k]   = real1 * real2 - imag1 * imag2; // 结果实部
        Result_Buffer[2*k+1] = real1 * imag2 + imag1 * real2; // 结果虚部
    }
}

void Freq_Multip_q15(q15_t *Complex_Buffer, q15_t *Sweep_Complex, q15_t *Result_Buffer, uint32_t size) 
{
    for (uint16_t k = 0; k < size / 2; k++) 
    {
        if(Complex_Buffer[2*k] < 5 && Complex_Buffer[2*k] > -5)
        {
            Complex_Buffer[2*k] = 0;
            Complex_Buffer[2*k+1] = 0;
            continue;
        }
        // 读取输入复数（Q15格式）
        q15_t a = Complex_Buffer[2*k];      // 输入信号实部
        q15_t b = Complex_Buffer[2*k+1];    // 输入信号虚部
        q15_t c = Sweep_Complex[2*k];       // 扫频信号实部
        q15_t d = Sweep_Complex[2*k+1];    // 扫频信号虚部

        // 计算中间乘法（32位精度）
        q31_t ac = (q31_t)a * c;  // a*c
        q31_t bd = (q31_t)b * d;  // b*d
        q31_t ad = (q31_t)a * d;  // a*d
        q31_t bc = (q31_t)b * c;  // b*c

        // 实部: ac - bd，虚部: ad + bc（32位结果）
        q31_t real_tmp = ac - bd;
        q31_t imag_tmp = ad + bc;

        // 右移15位 + 饱和处理（转回Q15）
        Result_Buffer[2*k]   = __SSAT(real_tmp >> 15, 16); // 实部
        Result_Buffer[2*k+1] = __SSAT(imag_tmp >> 15, 16); // 虚部
    }
}

void DDS_Convert_With_Outside(float freq, uint Amp_mv)
{
    uint Amp = (uint)(1.0 * Amp_mv / 10.2f);
    DAC_Set(1000);
    Freq_convert(freq);
    Write_Amplitude(Amp);
    Write_Phi(0);
}

void Amp_Adjust_FeedBack(uint16_t Amp_mv_set, float freq, uint16_t Current_Amp)
{
    getADCsamples();
    uint16_t vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
    vpp = Debug_Ratio_for_Adjust_Vpp * vpp * 3300 / 4095;
    uint16_t diff = abs(vpp - Amp_mv_set);
    while(diff > 10)
    {
        if(Amp_mv_set > vpp)
        {
            Current_Amp += 3;
            DDS_Convert_With_Outside(freq, Current_Amp);
        }
        else 
        {
            Current_Amp -= 3;
            DDS_Convert_With_Outside(freq, Current_Amp);
        }
        getADCsamples();
        vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
        vpp = vpp * Debug_Ratio_for_Adjust_Vpp * 3300 / 4095;
        diff = abs(vpp - Amp_mv_set);
        if(UART_flag == true) break;
    }
    // __BKPT();
    
}


void Amp_Adjust_FeedBack_Random(uint16_t Amp_mv_set, uint16_t DAC_gain)
{
    getADCsamples();
    uint16_t vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
    vpp = Debug_Ratio_for_HighFreq * vpp * 3300 / 4095;
    uint16_t diff = abs(vpp - Amp_mv_set);
    while(diff > 5)
    {
        if(Amp_mv_set > vpp)
        {
            DAC_gain += 1;
            DAC_Set(DAC_gain);
        }
        else 
        {
            DAC_gain -= 1;
            DAC_Set(DAC_gain);
        }
        if(DAC_gain > 1250 || DAC_gain < 400) break;
        if(Current_freq < 700 && DAC_gain > Debug_DAC_SET_LF)  break;
        getADCsamples();
        vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
        vpp = vpp * Debug_Ratio_for_HighFreq * 3300 / 4095;
        diff = abs(vpp - Amp_mv_set);
    }
    
}

void Amp_Adjust_FeedBack_Random_OutSignal(uint16_t Amp_mv_set, uint16_t DAC_gain)
{
    getADCsamples();
    uint16_t vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
    vpp = Debug_Ratio_for_OutSignal * vpp * 3300 / 4095;
    uint16_t diff = abs(vpp - Amp_mv_set);
    while(diff > 5)
    {
        if(Amp_mv_set > vpp)
        {
            DAC_gain += 1;
            DAC_Set(DAC_gain);
        }
        else 
        {
            DAC_gain -= 1;
            DAC_Set(DAC_gain);
        }
        if(DAC_gain > 1250 || DAC_gain < 400) break;
        if(Current_freq < 700 && DAC_gain > 1050)  break;
        getADCsamples();
        vpp = Find_Vpp_Int(gADCSamples_x, FFT_SIZE); 
        vpp = vpp * Debug_Ratio_for_OutSignal * 3300 / 4095;
        diff = abs(vpp - Amp_mv_set);
    }
    
}


void Output_1to2V_Sin(float freq, uint16_t Amp_mV)
{
    float HS = Cal_HsAmp(freq);
    uint Amp = (uint)(Amp_mV / HS);
    DDS_Convert_With_Outside(freq,Amp);
    Amp_Adjust_FeedBack(Amp, freq, Amp);
}

void Output_After_Learning(q15_t *Complex_Buffer,float Amp)
{
    float OUTPUT_Buffer[FFT_SIZE];
    uint16_t INT_Buffer[FFT_SIZE];
    //DDS_Convert_With_Outside(25000,2000);
    DAC_Set(900);
    delay_cycles(32000);
    getADCsamples();
    CFFT_ProcessADCData(&fftHandle, gADCSamples_y, FFT_SIZE);
    // if(fftHandle.maxIndex+3<10)
    // {
    //     change_ADCsample_rate(200000);
    //     delay_cycles(32000);
    //     getADCsamples();
    //     CFFT_ProcessADCData(&fftHandle, gADCSamples_y, FFT_SIZE);
    //     //请元宝写一个变换函数，把存在复数谱的200khz采样的X（s）变换为等效1000khz采样的X（s）'，
    //     //存在已有的static q15_t complexBuffer[2 * FFT_SIZE]; // 复数缓冲区，中间需要的临时变量进行自动生成
    //     convertTo1000kSpec( complexBuffer,  FFT_SIZE);
    //     change_ADCsample_rate(1000000);
    // }
    Freq_Multip_q15(complexBuffer,Buffer_sweep,Buffer_Muti_Result,FFT_SIZE);
    //float predicted_vpp = Calculate_Output_Vpp_From_Spectrum(Buffer_Muti_Result, FFT_SIZE);
   // __BKPT();
    for (uint16_t k = 0; k <= FFT_SIZE / 2 - 1; k++) {
        uint32_t src_idx = 2 * k;           // 正频率分量索引（实部位置）
        uint32_t dst_idx = 2 * (FFT_SIZE - k - 1);     // 对应负频率分量索引（实部位置）
                // 实部保持相同
        Buffer_Muti_Result[dst_idx] = Buffer_Muti_Result[src_idx];    
        // 虚部取负（共轭操作）
        Buffer_Muti_Result[dst_idx + 1] = -Buffer_Muti_Result[src_idx + 1];
    }
    generate_waveform_directly(&fftHandle,Buffer_Muti_Result, 10000,&max_value_after_ifft);
    float predicted_vpp = (float)(max_value_after_ifft / 200000000.0f);
    for(uint16_t i=FFT_SIZE/2;i<FFT_SIZE;i++)
    {
        OUTPUT_Buffer[i-FFT_SIZE/2] = ((float)inputBuffer[i] / 20000.0 )* Amp + 1.65;
        OUTPUT_Buffer[i] = 0;
    }
    //__BKPT();
    for(uint16_t i=FFT_SIZE/2;i<FFT_SIZE;i++)
    {
        INT_Buffer[i-FFT_SIZE/2] = (uint16_t)(OUTPUT_Buffer[i-FFT_SIZE/2]*1000);
        INT_Buffer[i] = 0;
    }
    //__BKPT();
    float dc_offset_y = Find_DC_OffSet_Input(INT_Buffer);
    float Sample_dots = calculate_period_in_samples(INT_Buffer, FFT_SIZE/2, dc_offset_y);

    float dc_offset_y_f = Find_DC_OffSet_Input(gADCSamples_y);
    float dc_offset_y_f_l = Find_DC_OffSet_Input_L(gADCSamples_y);
    float dc_offset_y_f_m = Find_DC_OffSet_Input_M(gADCSamples_y);
    float Sample_dots_f = calculate_period_in_samples(gADCSamples_y, FFT_SIZE/2, dc_offset_y_f);
    float Sample_dots_f_l = calculate_period_in_samples(gADCSamples_y, FFT_SIZE/2, dc_offset_y_f_l);
    float Sample_dots_f_m = calculate_period_in_samples(gADCSamples_y, FFT_SIZE/2, dc_offset_y_f_l);
    if(Sample_dots_f_l > Sample_dots_f && Sample_dots_f_l > Sample_dots_f_m) Sample_dots_f = Sample_dots_f_l;
    if(Sample_dots_f_m > Sample_dots_f && Sample_dots_f_m > Sample_dots_f_l) Sample_dots_f = Sample_dots_f_m;
    // uint32_t freq =  ((uint32_t)(1000000000.0 / (Sample_dots_f * SAMPLE_INTERVAL_ns)) + 100) / 200 * 200;

    uint32_t freq = Get_Freq_From_FreqBase();

    if(Reverse_flag){
        for(uint16_t i = 0;i<Sample_dots_f/2-1;i++)
        {
            float temp = OUTPUT_Buffer[i];
            OUTPUT_Buffer[i] = OUTPUT_Buffer[(int)Sample_dots_f-1-i];
            OUTPUT_Buffer[(int)Sample_dots_f-1-i] = temp;
        }
    }
    //Write_Amplitude(600);
    OutputArbitraryWaveform_NN(freq, OUTPUT_Buffer, (int)Sample_dots_f);
    Amp_Adjust_FeedBack_Random_OutSignal((uint16_t)(1000 * predicted_vpp), 900);
    //__BKPT();
}



/**
 * @brief 将200kHz采样的频谱转换为等效1000kHz采样频谱
 * @param complexBuffer 输入：200kHz频谱（实部+虚部交替存储）；输出：1000kHz频谱
 * @param fftSize FFT点数（固定1024）
 * @note 使用q15定点数，动态内存分配避免栈溢出
 */
void convertTo1000kSpec(q15_t* complexBuffer, uint32_t fftSize) {
    // 1. 频域合并：5个195.3Hz频点 -> 1个976.6Hz频点
    const uint32_t factor = 5; // 采样率提升倍数
    q31_t sum_real, sum_imag;  // 32位避免溢出
    
    // 2. 处理直流分量（0Hz）和Nyquist点（100kHz）
    // 保留原值并缩放（因分辨率降低需补偿能量）
    complexBuffer[0] = complexBuffer[0] / factor;      // DC实部
    complexBuffer[1] = complexBuffer[1] / factor;      // DC虚部
    complexBuffer[fftSize] = complexBuffer[fftSize] / factor;   // Nyquist实部
    complexBuffer[fftSize+1] = complexBuffer[fftSize+1] / factor; // Nyquist虚部

    // 3. 合并正频率部分（1 ~ 511）
    for (uint32_t newBin = 1; newBin < fftSize/2; newBin++) {
        sum_real = 0;
        sum_imag = 0;
        uint32_t startIdx = 2 * (newBin * factor);  // 源频谱索引
        
        // 累加5个原始频点（q15加法需转q31防溢出）
        for (uint32_t k = 0; k < factor; k++) {
            sum_real += (q31_t)complexBuffer[startIdx + 2*k];     // 实部
            sum_imag += (q31_t)complexBuffer[startIdx + 2*k + 1]; // 虚部
        }
        
        // 缩放并转回q15（等效分辨率降低）
        complexBuffer[2*newBin] = (q15_t)(sum_real / factor);   // 新实部
        complexBuffer[2*newBin+1] = (q15_t)(sum_imag / factor); // 新虚部
    }

    // 4. 处理负频率部分（对称性复制）
    for (uint32_t i = 1; i < fftSize/2; i++) {
        complexBuffer[2*(fftSize - i)] = complexBuffer[2*i];      // 实部对称
        complexBuffer[2*(fftSize - i)+1] = -complexBuffer[2*i+1]; // 虚部取反
    }
}

float Find_DC_OffSet_Input(uint16_t *Sample) 
{
    uint16_t min_val = UINT16_MAX; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < FFT_SIZE/2; i++) {
        uint16_t sample = Sample[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return (max_val * 0.9 + min_val * 0.1); // 
}

float Find_DC_OffSet_Input_L(uint16_t *Sample) 
{
    uint16_t min_val = UINT16_MAX; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < FFT_SIZE/2; i++) {
        uint16_t sample = Sample[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return (max_val * 0.2 + min_val * 0.8); // 
}

float Find_DC_OffSet_Input_M(uint16_t *Sample) 
{
    uint16_t min_val = UINT16_MAX; // 初始化为最大可能值
    uint16_t max_val = 0;          // 初始化为最小可能值

    // 单次遍历同时获取最小值和最大值
    for (int i = 0; i < FFT_SIZE/2; i++) {
        uint16_t sample = Sample[i];
        if (sample < min_val) min_val = sample; // 更新最小值
        if (sample > max_val) max_val = sample; // 更新最大值
    }
    return (max_val * 0.5 + min_val * 0.5); // 
}

uint32_t Get_Freq_From_FreqBase(void)
{
    change_ADCsample_rate(100000);
    getADCsamples();
    CFFT_ProcessADCData_hamming(&fftHandle, gADCSamples_y, FFT_SIZE);
    // uint32_t base_index = fftHandle.maxIndex + 3;
    // uint32_t freq = 97.656 * (float)base_index;
    change_ADCsample_rate(1000000);
    // return freq;
    // 获取基波信息
    float base_value = fftHandle.maxValue;
    float base_index = (float)(fftHandle.maxIndex + 3);//补偿直流分量
    // --- 新增：抛物线插值修正 ---
    if (base_index > 0 && base_index < (FFT_SIZE / 2 - 1)) {
        float y0 = fftHandle.magnitude[(uint32_t)(base_index - 1)];// 左邻点幅度
        float y1 = base_value;// 峰值点幅度
        float y2 = fftHandle.magnitude[(uint32_t)(base_index + 1)];// 右邻点幅度
        // 计算频偏系数δ（小数部分）
        float delta = (float)(y0 - y2) / (2.0f * (y0 - 2*y1 + y2));
        // 修正基波频率索引
        base_index += delta;
    }
    return ((uint32_t)(base_index*100000/1024/Debug_Index_Ratio) + 100)/200 * 200;

}

void change_ADCsample_rate(uint32_t sample_rate_hz)
{
    // 计算新周期值（确保在16位范围内）
    uint32_t new_period = (32000000 / sample_rate_hz) - 1;
    if(new_period > 0xFFFF) new_period = 0xFFFF; // 最大65535
    if(new_period == 0) new_period = 1;          // 最小1
    
    // 创建新的定时器配置
    DL_TimerG_TimerConfig newConfig = {
        .period     = (uint16_t)new_period,
        .timerMode  = DL_TIMER_TIMER_MODE_PERIODIC,
        .startTimer = DL_TIMER_STOP // 稍后手动启动
    };
    
    // 重新配置定时器
    DL_TimerG_stopCounter(TIMER_0_INST);       // 停止定时器
    DL_TimerG_initTimerMode(TIMER_0_INST, &newConfig); // 应用新配置
    
    // 保持其他设置不变
    DL_TimerG_enableEvent(TIMER_0_INST, DL_TIMERA_EVENT_ROUTE_1, DL_TIMERA_EVENT_ZERO_EVENT);
    DL_TimerG_setPublisherChanID(TIMER_0_INST, DL_TIMERA_PUBLISHER_INDEX_0, TIMER_0_INST_PUB_0_CH);
    
    //DL_TimerA_startCounter(TIMER_0_INST); // 重新启动
}

/**
 * @brief Q15格式的Savitzky-Golay平滑
 * @param input 输入数据数组（Q15格式，512点）
 * @param output 输出数据数组（Q15格式，512点）
 */
void savgol_smooth_q15(int16_t* input, int16_t* output) {
    const int n = 512;
    for (int i = 0; i < n; i++) {
        int32_t sum = 0;  // 32位中间变量防溢出
        for (int j = -2; j <= 2; j++) {
            int idx = i + j;
            // 镜像边界处理
            if (idx < 0) idx = -idx;
            else if (idx >= n) idx = 2 * n - idx - 1;
            
            // Q15乘法：input * coeff -> Q30
            sum += (int32_t)input[idx] * sg_coeffs_q15[j + 2];
        }
        // Q30转Q15：右移15位并饱和处理
        sum = (sum + 0x4000) >> 15;  // 四舍五入
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        output[i] = (int16_t)sum;
    }
}

/**
 * @brief 根据频域相乘的结果，计算输出信号的时域峰峰值电压(Vpp)。
 * @param Muti_Result_Complex_Buffer 频域相乘后的复数谱数组 (Buffer_Muti_Result)
 * @param size FFT的点数 (FFT_SIZE)
 * @return 预测的输出信号Vpp，单位为伏特(V)。
 */
float Calculate_Output_Vpp_From_Spectrum(q15_t *Muti_Result_Complex_Buffer, uint32_t size)
{
    const int N = size;
    q63_t max_raw_val = -9223372036854775807LL; // 64位有符号数的最小值
    q63_t min_raw_val = 9123372036854775808LL;  // 64位有符号数的最大值

    // --- 核心步骤：合成时域波形（不归一化） ---
    // 这个循环与您 generate_waveform_directly 的第一遍扫描逻辑完全相同
    for (int n = 0; n < N; n++) {
        q63_t acc = 0; // 使用64位累加器防止溢出

        // 从k=1开始循环到奈奎斯特频率
        for (int k = 1; k < N / 2; k++) {
            q15_t real_k = Muti_Result_Complex_Buffer[2 * k];
            q15_t imag_k = Muti_Result_Complex_Buffer[2 * k + 1];

            if (real_k == 0 && imag_k == 0) {
                continue;
            }

            // 计算相位角度 (0-32767 代表 0-2PI)
            q15_t angle = (q15_t)(((q31_t)n * k * 32768) / (N));
            q15_t cos_val = arm_cos_q15(angle);
            q15_t sin_val = arm_sin_q15(angle);
            
            // 根据反变换公式累加: y[n] = Σ (Real[k]*cos - Imag[k]*sin)
            // 乘以2补偿共轭对称的另一半
            acc += 2 * ((q63_t)real_k * cos_val - (q63_t)imag_k * sin_val);
        }

        // 寻找合成后时域信号的原始最大值和最小值
        if (acc > max_raw_val) {
            max_raw_val = acc;
        }
        if (acc < min_raw_val) {
            min_raw_val = acc;
        }
    }

    // 如果信号全为0，则Vpp为0
    if (max_raw_val == min_raw_val) {
        return 0.0f;
    }

    // --- 关键步骤：将原始的raw值转换为物理电压 ---
    // 1. IFFT反缩放: 标准IFFT需要除以N，而我们的手写IFFT没有，所以要在这里补上
    // 2. 频域乘法反缩放: Freq_Multip_q15 中右移了15位(>>15)，相当于除以32768
    // 3. FFT反缩放: arm_cfft_q15 乘以了 N
    // 4. ADC->Q15反缩放: CFFT_ProcessADCData 中乘以了 8
    // 综合起来，从 Muti_Result_Complex_Buffer (Q15) 到 时域ADC值(Q15) 的 IFFT 过程，
    // 其总的缩放因子是 (1/N) * 32768 / N / 8。
    // 但是我们手写的IFFT结果(acc)本身就比标准IFFT结果大了N倍，所以从acc到时域ADC值(Q15)的缩放是：
    // 时域ADC(q15) = acc / N 
    
    // 物理电压换算:
    // V_ac = ADC_ac_12bit * (3.3 / 4095)
    // ADC_ac_q15 = ADC_ac_12bit * 8  => ADC_ac_12bit = ADC_ac_q15 / 8
    // 因此: V_ac = (ADC_ac_q15 / 8) * (3.3 / 4095)
    
    // 结合以上两步：
    // V_ac[n] = ( (acc[n] / N) / 8 ) * (3.3 / 4095)
    // V_ac[n] = acc[n] * (3.3 / (N * 8 * 4095))

    const float V_REF = 3.3f;
    const float ADC_MAX_VAL = 4095.0f;
    const float ADC_TO_Q15_SCALE = 8.0f;
    
    float scale_to_volts = V_REF / ( (float)N * ADC_TO_Q15_SCALE * ADC_MAX_VAL );

    float v_max = (float)max_raw_val * scale_to_volts;
    float v_min = (float)min_raw_val * scale_to_volts;

    return v_max - v_min;
}



//----------------------------------END_OF_FILE---------------------------------