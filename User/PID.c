#include "pid.h"

extern volatile float gait_v;
PID Zhuanwan_PID;               //转向环PID

void PID_Init(void)          //PID初始化
{
	Zhuanwan_PID.Target_Val=0;
	Zhuanwan_PID.Output_Val=0.0;
	Zhuanwan_PID.Error=0.0;
	Zhuanwan_PID.LastError=0.0;
	Zhuanwan_PID.PrevError=0.0;
	Zhuanwan_PID.Integral=0.0;       
	Zhuanwan_PID.Kp=0.1;        
	Zhuanwan_PID.Ki=0.005;               
	Zhuanwan_PID.Kd=0.066;
}


float SpeedPID_Realise(PID *pid,float Actual_Val)
{
	pid->Error=pid->Target_Val - Actual_Val;
	//算法实现
	pid->Output_Val += pid->Kp * (pid->Error-pid->LastError) +
					   pid->Ki * pid->Error +
					   pid->Kd * (pid->Error-2*pid->LastError+pid->PrevError);
	//误差传递
	pid->PrevError=pid->LastError;
	pid->LastError=pid->Error;
	//输出限幅
	if(pid->Output_Val > Max_PID_Val)
	{
		pid->Output_Val = Max_PID_Val;
		return Max_PID_Val;
	}
	else if(pid->Output_Val < Min_PID_Val)
	{
		pid->Output_Val = Min_PID_Val;
		return Min_PID_Val;
	}
	else
	{
		return pid->Output_Val;
	}
}

float AnglePID_Realise(PID *pid, float Actual_val, float gyro)
{
	pid->Error = Actual_val - pid->Target_Val;

	/* 归一化到 ±180 */
	while(pid->Error >  180.0f) pid->Error -= 360.0f;
	while(pid->Error < -180.0f) pid->Error += 360.0f;

	pid->Integral += pid->Error;
	
	//if(gait_v>=0)
	//{
	//	if (pid->Integral >  800.0f) pid->Integral =  800.0f;//400
	//	if (pid->Integral < -800.0f) pid->Integral = -800.0f;
	//}
	//else
	//{
	//	if (pid->Integral >  600.0f) pid->Integral =  600.0f;
	//	if (pid->Integral < -600.0f) pid->Integral = -600.0f;
	//}
	/* D 项直接用陀螺仪角速度，不做差分
	 *   好处1：没有"启动冲击"（上次误差和这次无关）
	 *   好处2：不放大 Yaw 的量化噪声*/
	pid->Output_Val = pid->Kp * pid->Error +
	                  pid->Ki * pid->Integral +
	                  pid->Kd * gyro;

	/*pid->LastError = pid->Error;       原先是kd的系数，保留，其他地方可能用 */

	/* 输出限幅 */
	if(pid->Output_Val > Max_Zhuanwan_Val)       pid->Output_Val = Max_Zhuanwan_Val;
	else if(pid->Output_Val < Min_Zhuanwan_Val)  pid->Output_Val = Min_Zhuanwan_Val;

	return pid->Output_Val;
}
