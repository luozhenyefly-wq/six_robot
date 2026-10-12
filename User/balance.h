#ifndef __BALANCE_H
#define __BALANCE_H

#include <stdio.h>       
#include <math.h> 
#include "leg_angle.h"

/* ================= 姿态水平环 ================= */
#define LEVEL_LPF        0.30f    /* 一阶低通系数，越小越平滑 */
#define LEVEL_DB_DEG     0.5f     /* 死区，度 */
#define LEVEL_GAIN_P     1.3f     /* 俯仰增益：1.0 → 1.3 → 1.5 往上试 */
#define LEVEL_GAIN_R     1.3f     /* 横滚增益 */
#define LEVEL_SIGN_P     (+1.0f)  /* 俯仰符号 */
#define LEVEL_SIGN_R     (-1.0f)  /* 横滚符号 */
#define LEVEL_MAX_DEG_P   30.0f    /* 俯仰上限（上下坡），几何极限 ~40° */
#define LEVEL_MAX_DEG_R   17.0f    /* 横滚上限（侧坡），几何极限 ~23° */
#define LEVEL_KI_DEG     0.2f    /* 积分系数，每拍累积多少度 */
#define LEVEL_INT_MAX    20.0f    /* 积分限幅，度（要够大，否则补不满） */

void Level_Update(void);
void Level_Rotate(float *X, float *Y, float *Z);
void Level_Reset(void);

#endif
