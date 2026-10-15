#ifndef __ODOM_H
#define __ODOM_H

#include <stdint.h>
#include "leg_gait.h"

/* ================= 里程计（增量式） =================
 *
 * 分工：
 *   固件   = 只测量。累加"本拍沿朝向走了多远"，发送时原子取走并清零。
 *   上位机 = 做推算。用 Yaw / vYaw 把位移积成 x / y，做圆弧积分和蟹行补偿。
 *
 * 固件里没有三角函数、没有坐标变换 —— 那些都在上位机，
 * 改标定参数不用重烧程序，这是有意的分层。
 */

/* 每拍位移 = gait_v * ODOM_K_STRIDE / GAIT_N
 *   理论值 = 1 / (1 - GAIT_SWING_FRAC) = 1 / 0.65 = 1.538
 *   （摆动相占 0.35，支撑相只有 0.65 个周期，所以机身速度是 gait_v/0.65）
 *   实测标定：走 N 个周期、量实际距离 L，则 K = L / (gait_v * N) */
#define ODOM_K_STRIDE   1.538f

extern volatile float odom_ds;      /* 带符号位移增量 cm：前进为正、后退为负 */

void  Odom_Add(float ds);           /* 累加：步态任务每拍调用一次 */
float Odom_Take(void);              /* 取走并清零：发送任务每帧调用一次 */
void  Odom_Reset(void);             /* 手动复位（重新开始一段路径时调） */

#endif
