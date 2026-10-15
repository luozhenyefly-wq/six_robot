/*
	通过接收roll,pitch数据进行闭环调节，保持身体平衡
*/
#include "balance.h"

extern volatile float Pitch, Roll;

static float lvl_alpha = 0.0f;
static float lvl_beta  = 0.0f;
static float lvl_ip    = 0.0f;    /* 俯仰积分 */
static float lvl_ir    = 0.0f;    /* 横滚积分 */


void Level_Rotate(float *X, float *Y, float *Z)
{
    float ca = cosf(lvl_alpha), sa = sinf(lvl_alpha);
    float cb = cosf(lvl_beta),  sb = sinf(lvl_beta);

    float x = *X, y = *Y, z = *Z;

    /* 绕 Y 轴：俯仰 */
    float x1 =  x * ca - z * sa;
    float z1 =  x * sa + z * ca;

    /* 绕 X 轴：横滚（接着上一结果继续转） */
    float y1 =  y * cb + z1 * sb;
    float z2 = -y * sb + z1 * cb;

    *X = x1;
    *Y = y1;
    *Z = z2;
}

void Level_Update(void)
{
    static float pf = 0.0f;
    static float rf = 0.0f;
    float p, r;

    pf += LEVEL_LPF * (Pitch - pf);
    rf += LEVEL_LPF * (Roll  - rf);

    p = (pf >  LEVEL_DB_DEG) ? (pf - LEVEL_DB_DEG) :
        (pf < -LEVEL_DB_DEG) ? (pf + LEVEL_DB_DEG) : 0.0f;
    r = (rf >  LEVEL_DB_DEG) ? (rf - LEVEL_DB_DEG) :
        (rf < -LEVEL_DB_DEG) ? (rf + LEVEL_DB_DEG) : 0.0f;

    /* 积分：累积"死区之后"的偏差，把柔性造成的静差补掉。
       进了死区就停止累积 —— 天然防抖，不会来回蹭。*/
    lvl_ip += LEVEL_KI_DEG * p;
    lvl_ir += LEVEL_KI_DEG * r;

    if (lvl_ip >  LEVEL_INT_MAX) lvl_ip =  LEVEL_INT_MAX;
    if (lvl_ip < -LEVEL_INT_MAX) lvl_ip = -LEVEL_INT_MAX;
    if (lvl_ir >  LEVEL_INT_MAX) lvl_ir =  LEVEL_INT_MAX;
    if (lvl_ir < -LEVEL_INT_MAX) lvl_ir = -LEVEL_INT_MAX;

    /* balance.c，Level_Update() 末尾，替换掉现在的单轴限幅 */

    /* 输出 */
    lvl_alpha = LEVEL_SIGN_P * (p + lvl_ip) * (PI / 180.0f) * LEVEL_GAIN_P;
    lvl_beta  = LEVEL_SIGN_R * (r + lvl_ir) * (PI / 180.0f) * LEVEL_GAIN_R;

    /* ---- 合成限幅：椭圆 (α/P)2 + (β/R)2 ≤ 1 ----
       单轴各自合法 ≠ 组合合法：
           纯俯仰 35°   → 后腿 rr = 18.84  ?
           纯横滚 20°   → 中间腿 rr = 19.13 ??
           俯仰35+横滚20 → 后腿 rr = 22.52  ? 超 2.8
       超了 Leg_IK 就返回中位姿态，六条腿轮流跳 → 就是"疯掉"。
       椭圆的好处：纯俯仰能用满 P，纯横滚能用满 R，只有两个轴同时大时才缩。*/
    float u  = lvl_alpha / (LEVEL_MAX_DEG_P * (PI / 180.0f));
    float v  = lvl_beta  / (LEVEL_MAX_DEG_R * (PI / 180.0f));
    float dd = u * u + v * v;

    if (!(dd <= 1.0f))          /* 注意：NaN 也会走进来 */
    {
        if (dd > 1.0f)          /* 真的超了 → 等比缩放，方向不变 */
        {
            float s = 1.0f / sqrtf(dd);
            lvl_alpha *= s;
            lvl_beta  *= s;
        }
        else                    /* NaN（数学出问题）→ 直接归零 */
        {
            lvl_alpha = 0.0f;
            lvl_beta  = 0.0f;
        }
    }
}

/* 清掉积分记忆（换站位高度、切换模式、或者手动摆正之后调一次）*/
void Level_Reset(void)
{
    lvl_ip   = 0.0f;
    lvl_ir   = 0.0f;
    lvl_alpha = 0.0f;
    lvl_beta  = 0.0f;
}
