#ifndef __JY61P_H
#define __JY61P_H

#include "main.h"

extern volatile float Pitch, Roll, Yaw;
extern volatile float vYaw;

void JY61P_Init(void);
void Serial_JY61P_Zero_Yaw(void);
void Serial_JY61P_Enable_Gyro_Output(void);

#endif
