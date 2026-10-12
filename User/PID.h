#ifndef __PID_H_
#define __PID_H_

#include "main.h"
#include "string.h"
#include "leg_angle.h"

#define Max_PID_Val   1000
#define Min_PID_Val   0

#define Max_Zhuanwan_Val      8.0f
#define Min_Zhuanwan_Val     -8.0f

typedef struct
{
	int16_t Target_Val;    //目标值
	float Error;         //第k次误差
	float LastError;     //第k-1次误差
	float PrevError;     //第k-2次误差
	float Kp,Ki,Kd;
	float Integral;      //积分值
	float Output_Val;    //输出值
}PID;//定义结构体数据类型


float SpeedPID_Realise(PID *pid,float Actual_Val);
float AnglePID_Realise(PID *pid, float Actual_val, float gyro);
void PID_Init(void);

#endif
