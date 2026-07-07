#ifndef __FFT_H
#define __FFT_H

void FFT(uint16_t * ad_source);
void FFT_single_wave(uint16_t * ad_source);
void getVpp(uint16_t * ad_source);
float find_peak_mag(uint16_t center_index, uint8_t search_radius);
extern uint8_t wave1_isSin;
extern uint8_t wave2_isSin;



extern float base_wave_freq;
extern uint16_t base_wave_freq_index;
extern uint16_t base_wave_freq_int;
extern float base_wave_phase;

extern float wave1_freq;
extern float wave2_freq;

extern float wave1_phase;
extern float wave2_phase;

extern uint16_t wave1_freq_index;
extern uint16_t wave2_freq_index;

extern uint16_t wave1_freq_int;
extern uint16_t wave2_freq_int;


extern uint16_t max;
extern uint16_t min;
extern uint16_t Vpp;
extern uint16_t Vdc;
extern float THD;

extern float relative_f2;//其他次谐波
extern float relative_f3; 
extern float relative_f4;
extern float relative_f5;
extern float relative_f6;
extern float relative_f7;
extern float FFT_output[];

extern uint32_t Fs;


#endif
