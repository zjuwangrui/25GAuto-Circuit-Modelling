#ifndef __IC_H
#define __IC_H

void IC_Init(void);
uint32_t IC_GetFreq(void);
float IC_GetDuty(void);
float IC_getPhaseDiff(float freq);
void IC2_Init(void);

#endif
