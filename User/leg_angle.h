#ifndef __LEG_ANGLE_H
#define __LEG_ANGLE_H

#include <math.h>
#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "leg.h"

#define PI 3.14159265358979323846f

// 左右腿
#define LEFT_LEG    (1)
#define RIGHT_LEG   (2)


// 保存三个关节角度
typedef struct
{
    float theta1;//J1 的转动角（底座 yaw）
    float theta2;//J2 的转动角（髋）
    float theta3;//J3 的相对转动角（膝）
    float theta1_real;//J1 舵机的实际输出角度
    float theta2_real;//J2 舵机的实际输出角度
    float theta3_real;//J3 舵机的实际输出角度
} LegAngle;

// 一条腿在机体坐标系下的安装参数
typedef struct
{
    float mx;//各条腿J1 转轴在机体坐标下的位置
    float my;
    float mz;
    float psi;//安装方位角（度）
    float L1;//髋段
    float L2;//大腿
    float L3;//小腿
} LegMount;


float rad_to_deg(float rad);
float deg_to_rad(float deg);
void LEG_Init(void);
LegAngle Leg_IK(float x, float y, float z,
                float L1, float L2, float L3,
                int solution, int left_or_right);
void Body_To_Leg(LegMount *leg, float X, float Y, float Z,
                 float *x, float *y, float *z);
LegAngle Leg_Solve(LegMount *leg, float X, float Y, float Z,
                   int solution, int left_or_right);

#endif
