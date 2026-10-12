/* ============================================================================
 * face.c  --  OLED 动态表情
 *
 * 【显存布局】OLED_DisplayBuf[8][128] 是页式：8 页 x 128 列，每页 8 像素高
 *
 * 【性能】软件 I2C（OLED_I2C_DELAY_US = 2），每字节约 100us
 *         全屏刷新 1024 字节约 100ms，太慢不能做动画
 *         所以只刷表情区，而且只在内容变化时才刷
 *
 * 【配色】OLED 是单色，只有"亮"和"暗"两种
 *         眼球(眼白) = 实心【亮】
 *         眼珠       = 在眼球上挖出的【暗】块（偏在一边，不是居中的圆瞳孔）
 *
 * 【眼睛的五种画法】
 *   style 0  平常 / 惊讶 / 傻子：实心亮眼球 + 挖一块黑
 *   style 1  开心：向上半椭圆（∩），擦掉下半
 *   style 2  难过：向下半椭圆（∪）+ 八字眉（外低内高）
 *   style 3  生气：向下半椭圆（∪）+ 倒八字眉（外高内低）
 *   style 4  困了：向下半椭圆（∪），无眉毛
 *
 *   1~4 都是"画实心椭圆 + 擦掉一半"，高度取 EYE_B × open，
 *   所以眨眼对它们同样有效（眼睛会跟着变扁）。
 *   不用 OLED_DrawArc —— 那条路在本工程里表现不稳。
 *
 * 【眨眼】靠"眼睑"把眼睛上下擦掉；style 1~4 则直接让椭圆变矮
 *
 * 【布局】表情区 X 16~111, Y 2~57
 *
 * 【注意】C89：所有变量声明放在语句块开头
 * ==========================================================================*/

#include "face.h"
#include "OLED.h"
#include <math.h>               /* sqrtf */

/* ---- 表情区 ---- */
#define FACE_X      16
#define FACE_Y      2
#define FACE_W      96
#define FACE_H      56

/* ---- 眼睛 ---- */
#define EYE_LX      (FACE_X + 20)   /* 36 */
#define EYE_RX      (FACE_X + 76)   /* 92 */
#define EYE_CY      (FACE_Y + 22)   /* 24 */
#define EYE_A       14              /* 横半轴 */
#define EYE_B       15              /* 纵半轴 */

/* ---- 嘴巴 ---- */
#define MOUTH_X     (FACE_X + 48)   /* 64，同时也是左右脸的分界 */
#define MOUTH_Y     (FACE_Y + 46)   /* 48 */

/* ---- 腮红 ---- */
#define BLUSH_LX    (FACE_X + 4)                /* 20，左腮红最左端 */
#define BLUSH_RX    (FACE_X + FACE_W - 4)       /* 108，右腮红最右端 */
#define BLUSH_Y     (FACE_Y + 35)               /* 37 */
#define BLUSH_H     6                           /* 斜线高度（固定） */
#define BLUSH_STEP_MS   180u                    /* 浮动动画每帧时长 */

/* 哪些表情带腮红 */
#define FACE_HAS_BLUSH(e)   ( ((e) == FACE_NORMAL)   || ((e) == FACE_HAPPY)    || \
                              ((e) == FACE_SAD)      || ((e) == FACE_SURPRISE) || \
                              ((e) == FACE_SLEEP)    || ((e) == FACE_SHAZI) )

/* ---- 眨眼参数 ---- */
#define FACE_BLINK_MIN_MS   2500u
#define FACE_BLINK_MAX_MS   5000u
#define FACE_BLINK_STEP_MS  40u

/* ---- 状态 ---- */
static uint8_t  faceEmotion   = FACE_NORMAL;
static uint8_t  faceDirty     = 1u;
static uint16_t faceBlinkWait = 3000u;
static uint8_t  faceBlinkStep = 0u;

static uint32_t faceTick      = 0u;     /* 上次 Face_Update 的时刻 */
static uint32_t faceStepTick  = 0u;     /* 上次眨眼阶段切换的时刻 */

static uint8_t  eyeOut        = 0u;     /* 0 = 眼珠朝内（平常）  1 = 对角（傻子）*/

static uint8_t  blushPhase    = 0u;     /* 腮红浮动相位 0~5 */
static uint32_t blushTick     = 0u;     /* 上次腮红动画切换的时刻 */

/* 腮红整体上下平移：0 -> +3 -> 0，六个相位来回 */
static const int8_t blushDY[6] = { 0, 1, 2, 3, 2, 1 };


/* ---------------------------------------------------------------------------
 * 简易伪随机（用来决定下次眨眼的间隔）
 * -------------------------------------------------------------------------*/
static uint16_t Face_Rand(void)
{
    static uint16_t seed = 0xACE1u;

    seed = (uint16_t)((seed << 1) ^ (((seed >> 15) & 1u) ? 0xB400u : 0u));
    seed ^= (uint16_t)HAL_GetTick();

    return seed;
}


/* ---------------------------------------------------------------------------
 * 挖一个圆形暗区
 *   库里只有矩形擦除，所以逐行拼出一个圆
 * -------------------------------------------------------------------------*/
static void Face_ClearCircle(int16_t cx, int16_t cy, int16_t r)
{
    int16_t dy;
    int16_t dx;

    if (r < 1) return;

    for (dy = -r; dy <= r; dy++)
    {
        dx = (int16_t)sqrtf((float)(r * r - dy * dy));

        OLED_ClearArea((int16_t)(cx - dx), (int16_t)(cy + dy),
                       (uint8_t)(dx * 2 + 1), 1);
    }
}


/* ---------------------------------------------------------------------------
 * 画一只眼睛
 *   open  : 0.0 = 完全闭上，1.0 = 完全睁开
 *   style : 0 = 实心眼球 + 挖黑块
 *           1 = 开心（∩）    2 = 难过（∪ + 八字眉）
 *           3 = 生气（∪ + 倒八字眉）   4 = 困了（∪，无眉）
 * -------------------------------------------------------------------------*/
static void Face_DrawEye(int16_t cx, int16_t cy, float open, uint8_t style)
{
    int16_t b;

    /* 眼球纵半轴，随眨眼变化 —— 所有画法都用它 */
    b = (int16_t)((float)EYE_B * open + 0.5f);

    /* 完全闭眼：一条横线 */
    if (b < 2)
    {
        OLED_DrawLine((int16_t)(cx - EYE_A), cy, (int16_t)(cx + EYE_A), cy);
        return;
    }

    /* ================= style 1~4：半椭圆眼 ================= */
    if (style != 0u)
    {
        OLED_DrawEllipse(cx, cy, EYE_A, (uint8_t)b, OLED_FILLED);

        if (style == 1u)
        {
            /* 开心：擦掉下半，留拱形 ∩ */
            OLED_ClearArea((int16_t)(cx - EYE_A - 1),
                           cy,
                           (uint8_t)(EYE_A * 2 + 3),
                           (uint8_t)(b + 2));
        }
        else
        {
            /* 难过 / 生气 / 困了：擦掉上半，留碗形 ∪ */
            OLED_ClearArea((int16_t)(cx - EYE_A - 1),
                           (int16_t)(cy - b - 1),
                           (uint8_t)(EYE_A * 2 + 3),
                           (uint8_t)(b + 1));

            if (style == 2u)
            {
                /* 难过：八字眉，外侧低、靠鼻侧高 */
                if (cx < MOUTH_X)
                {
                    OLED_DrawLine((int16_t)(cx - EYE_A), (int16_t)(cy - 3),
                                  (int16_t)(cx + EYE_A), (int16_t)(cy - 8));
                }
                else
                {
                    OLED_DrawLine((int16_t)(cx + EYE_A), (int16_t)(cy - 3),
                                  (int16_t)(cx - EYE_A), (int16_t)(cy - 8));
                }
            }
            else if (style == 3u)
            {
                /* 生气：倒八字眉，外侧高、靠鼻侧低 */
                if (cx < MOUTH_X)
                {
                    OLED_DrawLine((int16_t)(cx - EYE_A), (int16_t)(cy - 8),
                                  (int16_t)(cx + EYE_A), (int16_t)(cy - 3));
                }
                else
                {
                    OLED_DrawLine((int16_t)(cx + EYE_A), (int16_t)(cy - 8),
                                  (int16_t)(cx - EYE_A), (int16_t)(cy - 3));
                }
            }
            /* style 4（困了）：和难过同款碗形眼，但不画眉毛 */
        }
        return;
    }

    /* ================= style 0：平常 / 惊讶 / 傻子 ================= */

    /* 1. 眼球：实心亮 */
    OLED_DrawEllipse(cx, cy, EYE_A, (uint8_t)b, OLED_FILLED);

    /* 2. 挖一块黑当眼珠
     *    半径 5、偏移 5（对角时最远点 (10,10)），椭圆判据 0.95 < 1，不出界
     *    【不做镜像】—— 左眼和右眼各按自己的规则来，位置才不会看着别扭 */
    if (eyeOut)
    {
        /* 傻子：左眼眼珠在左上，右眼眼珠在右下（对角） */
        if (cx < MOUTH_X)
        {
            Face_ClearCircle((int16_t)(cx - 5), (int16_t)(cy - 5), 5);
        }
        else
        {
            Face_ClearCircle((int16_t)(cx + 5), (int16_t)(cy + 5), 5);
        }
    }
    else
    {
        /* 平常 / 惊讶：两块眼珠都在水平方向上朝内（靠鼻子） */
        if (cx < MOUTH_X)
        {
            Face_ClearCircle((int16_t)(cx + 5), cy, 5);     /* 左眼：靠右 */
        }
        else
        {
            Face_ClearCircle((int16_t)(cx - 5), cy, 5);     /* 右眼：靠左 */
        }
    }

    /* 3. 眼睑：把超出 open 高度的部分擦掉 */
    if (b < EYE_B)
    {
        OLED_ClearArea((int16_t)(cx - EYE_A),
                       (int16_t)(cy - EYE_B),
                       (uint8_t)(EYE_A * 2 + 1),
                       (uint8_t)(EYE_B - b));

        OLED_ClearArea((int16_t)(cx - EYE_A),
                       (int16_t)(cy + b + 1),
                       (uint8_t)(EYE_A * 2 + 1),
                       (uint8_t)(EYE_B - b));
    }
}


/* ---------------------------------------------------------------------------
 * ω 形嘴（猫嘴）：两个相连的下半弧
 *   相当于把数字 "3" 顺时针转 90 度
 * -------------------------------------------------------------------------*/
static void Face_DrawOmega(void)
{
    int16_t r;

    r = 6;                                  /* 每个半圆的半径 */

    /* 1. 两个并排的空心圆（必须是 UNFILLED —— 实心会变成两颗牙）*/
    OLED_DrawCircle((int16_t)(MOUTH_X - r), MOUTH_Y, (uint8_t)r, OLED_UNFILLED);
    OLED_DrawCircle((int16_t)(MOUTH_X + r), MOUTH_Y, (uint8_t)r, OLED_UNFILLED);

    /* 2. 一次性擦掉整个上半区，留下两道相连的下半弧
     *    分开擦会在中间接缝处留缺口，所以合并成一次
     *    高度只擦 r（不是 r+1），让弧的端点留在 Y = MOUTH_Y 上 */
    OLED_ClearArea((int16_t)(MOUTH_X - 2 * r), (int16_t)(MOUTH_Y - r),
                   (uint8_t)(4 * r + 1), (uint8_t)r);
}


/* ---------------------------------------------------------------------------
 * 画嘴巴
 * -------------------------------------------------------------------------*/
static void Face_DrawMouth(uint8_t emotion)
{
    switch (emotion)
    {
        case FACE_SAD:          /* 难过：小撇嘴 */
            OLED_DrawArc(MOUTH_X, (int16_t)(MOUTH_Y + 2), 9, 205, 335, OLED_UNFILLED);
            break;

        case FACE_ANGRY:        /* 生气：抿紧了，故意不画 */
            break;

        case FACE_SURPRISE:     /* 惊讶：张大嘴 */
            OLED_DrawCircle(MOUTH_X, MOUTH_Y, 8, OLED_FILLED);
            break;

        case FACE_SLEEP:        /* 困了：不画嘴 */
            break;

        default:                /* 平常 / 开心 / 傻子：ω 猫嘴 */
            Face_DrawOmega();
            break;
    }
}


/* ---------------------------------------------------------------------------
 * 腮红：脸颊两侧各三道斜线，整体上下浮动
 * -------------------------------------------------------------------------*/
static void Face_DrawBlush(void)
{
    static const uint8_t lineX[3] = { 0u, 5u, 10u };    /* 三道线的横向偏移 */
    int16_t y;
    uint8_t i;

    y = (int16_t)(BLUSH_Y + blushDY[blushPhase % 6u]);  /* 整体上下平移 */

    for (i = 0u; i < 3u; i++)
    {
        /* 左脸颊：三道 "\" */
        OLED_DrawLine((int16_t)(BLUSH_LX + lineX[i]), y,
                      (int16_t)(BLUSH_LX + lineX[i] + 4), (int16_t)(y + BLUSH_H));

        /* 右脸颊：三道 "/" （镜像）*/
        OLED_DrawLine((int16_t)(BLUSH_RX - lineX[i]), y,
                      (int16_t)(BLUSH_RX - lineX[i] - 4), (int16_t)(y + BLUSH_H));
    }
}


/* ---------------------------------------------------------------------------
 * 重绘整张脸（只写显存，不发送）
 * -------------------------------------------------------------------------*/
static void Face_Render(void)
{
    float   open;
    uint8_t style = 0u;

    OLED_ClearArea(FACE_X, FACE_Y, FACE_W, FACE_H);

    /* --- 眨眼：open 从 1.0 降到 0 再回到 1.0 --- */
    switch (faceBlinkStep)
    {
        case 1u: open = 0.70f; break;
        case 2u: open = 0.35f; break;
        case 3u: open = 0.00f; break;
        case 4u: open = 0.35f; break;
        case 5u: open = 0.70f; break;
        default: open = 1.00f; break;
    }

    /* --- 每组表情的默认值 --- */
    eyeOut = 0u;

    /* --- 按表情选画法 --- */
    switch (faceEmotion)
    {
        case FACE_HAPPY:    style  = 1u; break;
        case FACE_SAD:      style  = 2u; break;
        case FACE_ANGRY:    style  = 3u; break;
        case FACE_SLEEP:    style  = 4u; break;
        case FACE_SHAZI:    eyeOut = 1u; break;

        /* 平常 / 惊讶：用默认的 style 0 + 眼珠朝内 */
        default:            break;
    }

    Face_DrawEye(EYE_LX, EYE_CY, open, style);
    Face_DrawEye(EYE_RX, EYE_CY, open, style);
    Face_DrawMouth(faceEmotion);

    if (FACE_HAS_BLUSH(faceEmotion))
    {
        Face_DrawBlush();
    }
}


/* --------------------------------------------------------------------------- */

void Face_Init(void)
{
    OLED_Clear();

    faceEmotion   = FACE_NORMAL;
    faceBlinkWait = 3000u;
    faceBlinkStep = 0u;
    faceDirty     = 1u;
    eyeOut        = 0u;
    blushPhase    = 0u;
    blushTick     = HAL_GetTick();
    faceTick      = blushTick;
    faceStepTick  = faceTick;

    Face_Render();
    OLED_UpdateArea(FACE_X, FACE_Y, FACE_W, FACE_H);
}


void Face_SetEmotion(uint8_t emotion)
{
    if (faceEmotion != emotion)
    {
        faceEmotion = emotion;
        faceDirty   = 1u;
    }
}


void Face_Update(void)
{
    uint32_t now;
    uint32_t dt;

    now = HAL_GetTick();
    dt  = now - faceTick;
    faceTick = now;

    /* --- 腮红上下浮动（只有带腮红的表情才跑）--- */
    if (FACE_HAS_BLUSH(faceEmotion))
    {
        if ((now - blushTick) >= BLUSH_STEP_MS)
        {
            blushTick = now;
            blushPhase++;
            if (blushPhase >= 6u) blushPhase = 0u;
            faceDirty = 1u;
        }
    }
    else
    {
        blushPhase = 0u;
        blushTick  = now;
    }

    /* --- 眨眼计时（用真实流逝的毫秒，不按调用次数估）--- */
    if (faceBlinkStep == 0u)
    {
        if (faceBlinkWait > dt)
        {
            faceBlinkWait -= (uint16_t)dt;
        }
        else
        {
            faceBlinkStep = 1u;
            faceStepTick  = now;
            faceDirty     = 1u;
        }
    }
    else if ((now - faceStepTick) >= FACE_BLINK_STEP_MS)
    {
        faceStepTick  = now;
        faceBlinkStep++;
        faceDirty     = 1u;

        if (faceBlinkStep > 5u)
        {
            faceBlinkStep = 0u;
            faceBlinkWait = (uint16_t)(FACE_BLINK_MIN_MS +
                             (Face_Rand() % (FACE_BLINK_MAX_MS - FACE_BLINK_MIN_MS)));
        }
    }

    /* --- 只有内容变了才重画 --- */
    if (faceDirty)
    {
        faceDirty = 0u;

        Face_Render();
        OLED_UpdateArea(FACE_X, FACE_Y, FACE_W, FACE_H);
    }
}
