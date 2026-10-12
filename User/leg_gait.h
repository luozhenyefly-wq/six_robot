#ifndef __LEG_GAIT_H
#define __LEG_GAIT_H

#include "leg_angle.h"
#include "PID.h"
#include <stdio.h>              /* sprintf */
#include <math.h>               /* sinf, cosf */
#include "balance.h"
#include "odom.h"

#define STAND       (0)
#define MOVE        (1)
#define GAIT_FRAME_GAP   15u	//串口发送间隔(防止驱动板串口缓冲区爆满)
#define GAIT_N           8u	//运动的帧数//8，1000//12，1400
#define GAIT_CYCLE_MS    1000u  //运动周期，ms
#define GAIT_T_FRAME     (GAIT_CYCLE_MS / GAIT_N)   //一帧动作需要的时间   
#define GAIT_OVERHEAD    (24u + GAIT_FRAME_GAP)     //串口发送消息总耗时，24ms是串口两次发送的所需时间
#define GAIT_DWELL       (GAIT_T_FRAME - GAIT_OVERHEAD) //缓冲时间，也就是到下一次动作帧发送的间隔
#define GAIT_SWING_FRAC   0.35f   //摆动相占整个周期的占比
#define GAIT_Z_STAND     (-6.0f)  //站立时脚端 Z，机身离地 60mm 
#define GAIT_Z_LIFT      (-1.5f)  //摆动相抬腿高度，离地 45mm
#define YAW_TURN_SIGN    (1.0f)  //在pid里面要用到的方向常量
#define HOLD_ENTER_DEG   1.5f      /* 进这个范围就停步 */
#define HOLD_EXIT_DEG    3.5f      /* 偏离超过这个才重新转 */

extern volatile float   gait_v;
extern volatile float   gait_turn;
extern volatile uint8_t gait_pid_en;
extern volatile float   gait_yaw_target;
extern volatile uint8_t gait_stop_req; 

// 一个舵机目标：舵机号 + 目标角度（0 ~ SERVO_MAX_DEG）。
typedef struct
{
    uint8_t id;
    float   deg;
} GaitServo;

typedef struct
{
    LegMount *mnt;
    uint8_t   id[3];        /* J1(偏航)、J2(髋)、J3(膝) */
    float     X0;           /* 站立位 X */
    float     Y0;           /* 站立位 Y */
    int       lr;           /* LEFT_LEG / RIGHT_LEG */
    uint8_t   grp;          /* 0 = A 组，1 = B 组 */
} GaitLeg;

void gait_init(GaitServo *list,volatile uint8_t *mode);
void leg_gait(GaitServo *list,volatile uint8_t *mode,volatile uint8_t *want_mode);

#endif
