#ifndef __FILTER_H
#define __FILTER_H
#include "arm_math.h"
void initIIRFilter(float a2,float a1,float a0,float b2,float b1,float b0,uint32_t fs);


void processIIRFilter(float32_t *input, float32_t *output, uint32_t blockSize);

#endif
