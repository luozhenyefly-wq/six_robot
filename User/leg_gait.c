/* ============================================================================
   leg_gait.c  --  六足三角步态（tripod gait）
   ============================================================================*/
#include "leg_gait.h"

extern LegMount LEG1, LEG2, LEG3, LEG4, LEG5, LEG6;
extern PID Zhuanwan_PID;
extern volatile float Yaw;
extern volatile float vYaw;

volatile float gait_v    = 0.0f;   /* 前进步幅 cm，负 = 后退   *///移动12
volatile float gait_turn =  0.0f;   /* 转弯步幅 cm，正 = 右转   *///原地转弯8
volatile uint8_t gait_pid_en     = 1;      //0：开环模式//1：闭环模式
volatile float   gait_yaw_target = 0.0f;   //目标角度	
volatile uint8_t gait_mode;
volatile uint8_t gait_want_mode=STAND;//移动模式
/* ---- 停车 ---- */
volatile uint8_t gait_stop_req = 0;     /* 外部置 1 请求停车 */
static   uint8_t gait_stopping = 0;     /* 内部状态：停车流程进行中 */

static uint8_t gait_hold = 0u;		   //转弯反馈标志位
/* ---------------------------------------------------------------------------
 * 每条腿的配置
 * -------------------------------------------------------------------------*/
 
static const GaitLeg leg_cfg[6] =
{
    { &LEG1, { 1,  2,  3},   18.0f,  18.0f,  LEFT_LEG , 0 },   /* 前左 A */
    { &LEG2, { 4,  5,  6},    0.0f,  25.5f,  LEFT_LEG , 1 },   /* 中左 B */
    { &LEG3, { 7,  8,  9},  -18.0f,  18.0f,  LEFT_LEG , 0 },   /* 后左 A */
    { &LEG4, {10, 11, 12},   18.0f, -18.0f,  RIGHT_LEG, 1 },   /* 前右 B */
    { &LEG5, {13, 14, 15},    0.0f, -25.5f,  RIGHT_LEG, 0 },   /* 中右 A */
    { &LEG6, {16, 17, 18},  -18.0f, -18.0f,  RIGHT_LEG, 1 }    /* 后右 B */
};//这里记录的是每条腿站立状态的数据，也是腿部摆动的中立位置

/* ---------------------------------------------------------------------------
 * 单腿足端轨迹，参数 s 是这条腿自己的归一化相位
 *
 * 支撑相必须保持匀速：脚踩在地上不动时，机身是按固定速度前进的，
 * 这里加缓动会让整个机器人一顿一顿。所有"仿生感"的修饰都应该放在摆动相。
 *
 * 椭圆弧的足部轨迹是"抬起"和"前摆"同时进行，走出来的是一条圆弧，
 * -------------------------------------------------------------------------*/
static void GaitTraj(float s, float *d, float *Z)
{
	//GAIT_SWING_FRAC:摆动相在整个周期的占比,s代表的是当前运动进度
    if (s < GAIT_SWING_FRAC)						  //s<GAIT_SWING_FRAC,处于摆动相
    {
        float u  = s / GAIT_SWING_FRAC;               //摆动相进度 0 -> 1 
        float lz = sinf(PI * u);                      // 抬升剖面 

        *d = -0.5f * cosf(PI * u);					  //*d=[-0.5,+0.5],这是一个当前足部摆动位置的量，*d=0的时候在中立位
													  //此处不直接使用*d=-0.5+u,是为了摆动的仿生感，两边慢中间快
		                                              //*d具体用法见GaitPose函数
        *Z = GAIT_Z_STAND + (GAIT_Z_LIFT - GAIT_Z_STAND) * lz * lz;
    }
    else
    {
        float v = (s - GAIT_SWING_FRAC) / (1.0f - GAIT_SWING_FRAC);                  /* 支撑相进度 0 -> 1 */

        *d = 0.5f - v;								  //支撑相保持匀速
        *Z = GAIT_Z_STAND;
    }
}

/* ---------------------------------------------------------------------------
 * 拼一帧带 {} 的组指令并发出去，最多 9 个舵机。
 *   '{' + 9 * 15 + '}' + 结束符 = 138 字节
 * 用静态缓冲，不占任务栈。
 * -------------------------------------------------------------------------*/
static char GaitBuf[2 + 15 * 9 + 1];

static void GaitSend(const GaitServo *list, uint8_t n, uint16_t ms)
{
    uint8_t i;
    int     len = 0;

    if (n  > 9u)    n  = 9u;
    if (ms > 9999u) ms = 9999u;

    GaitBuf[len++] = '{';

    for (i = 0; i < n; i++)
    {
        uint16_t pwm;
        float    d = list[i].deg;

        // 角度越界就跳过这个关节，而不是悄悄夹到行程尽头。
        //   少动一个关节远比把舵机顶死在极限位置容易发现和排查。 
        if (d < 0.0f || d > (float)SERVO_MAX_DEG)
        {
            continue;
        }

        pwm = (uint16_t)SERVO_PWM((uint16_t)(d + 0.5f));
        if (pwm < 500u)  pwm = 500u;
        if (pwm > 2500u) pwm = 2500u;

        len += sprintf(&GaitBuf[len], "#%03uP%04uT%04u!",(unsigned)list[i].id, (unsigned)pwm, (unsigned)ms);                       
    }

    GaitBuf[len++] = '}';
    GaitBuf[len]   = '\0';

    Servo_SendCmd(GaitBuf);
}

static float YawPid(void)
{
    Zhuanwan_PID.Target_Val = (int16_t)gait_yaw_target;
	
	if (gait_v < 0.0f)//后退pid参数
    {
       Zhuanwan_PID.Kp = 0.22f;//0.16//18
       Zhuanwan_PID.Ki = 0.028f;//0.010//15
       Zhuanwan_PID.Kd = 0.140f;//0.11//126
    }
	else if(gait_v == 15.0f)//高速前进，不稳定，慎用
	{
		Zhuanwan_PID.Kp=0.1f;            
		Zhuanwan_PID.Ki=0.02f;               
		Zhuanwan_PID.Kd=0.066f;
	}
	else if(gait_v == 10.0f)//正常前进，比较稳定
	{
		Zhuanwan_PID.Kp=0.22f; //0.24 //0.5 //0.3 //0.22
		Zhuanwan_PID.Ki=0.028f;//0.025//0.05//0.05//0.028
		Zhuanwan_PID.Kd=0.130f;//0.168//0.4 //0.2 //0.130
	}
	else//停止状态，温和一点的参数
	{
		Zhuanwan_PID.Kp = 0.20f;//0.12
		Zhuanwan_PID.Ki = 0.015f;
		Zhuanwan_PID.Kd = 0.08f;
	}
    return YAW_TURN_SIGN * AnglePID_Realise(&Zhuanwan_PID, Yaw,vYaw);
}

/* ---------------------------------------------------------------------------
 * 把 gl[base]、gl[base+1]、gl[base+2] 这三条腿在全局相位 s 时的姿态
 * 填进 list[0..8]。每条腿自己加上所属组的相位偏移。
 * -------------------------------------------------------------------------*/
static void GaitPose(GaitServo *list, uint8_t base, float s, uint8_t mode)
{
    uint8_t i;
	const GaitLeg *L  = NULL;
    for (i = 0; i < 3; i++)
    {
		L = &leg_cfg[base + i];  
        float          ls = L->grp ? (s + 0.5f) : s;   /* 这条腿自己的相位 */
        float          X,Y,Z,d,fx,fy;
        LegAngle       a;

        if (ls >= 1.0f)
        {
            ls -= 1.0f;
        }
				
		if (mode == STAND)
        {
            // 站立：不迈步、不抬腿 
            X = L->X0;
            Y = L->Y0;
            Z = GAIT_Z_STAND;
        }
        else
        {
            // ---- 核心 6 行：方向 + 步幅现算 ---- 
            float rr = sqrtf(L->X0 * L->X0 + L->Y0 * L->Y0);

            fx = gait_v + gait_turn * ( L->Y0 / rr);    // 前进 + 转弯X 
            fy =         - gait_turn * ( L->X0 / rr);   //  只有转弯Y   
			
			 if (gait_hold)                 
            {
                X = L->X0;  Y = L->Y0;  Z = GAIT_Z_STAND;
            }
			else
			{
				GaitTraj(ls, &d, &Z);        // d ∈ [-0.5, +0.5] 

				X = L->X0 + d * fx;			//假如fx是12cm,fx*d之后就代表腿部会在相对于中立位[-6,+6]的位置移动
				Y = L->Y0 + d * fy; 			//此处同理，L->X0，L->Y0就是中立位，此处可得到绝对位置X,Y
			}        
        }
		Level_Rotate(&X, &Y, &Z);  
		//Z += Level_Z(L->X0, L->Y0); 
		
		a = Leg_Solve(L->mnt, X, Y, Z, -1, L->lr);

        list[i * 3 + 0].id  = L->id[0];
        list[i * 3 + 0].deg = a.theta1_real;
        list[i * 3 + 1].id  = L->id[1];
        list[i * 3 + 1].deg = a.theta2_real;
        list[i * 3 + 2].id  = L->id[2];
        list[i * 3 + 2].deg = a.theta3_real;
    }
}

void gait_init(GaitServo *list,volatile uint8_t *want_mode)
{	
	uint8_t base;
    for (base = 0; base <= 3; base += 3)
    {
        GaitPose(list, base, 0.0f,*want_mode);
        GaitSend(list, 9, 600);
        osDelay(15);
    }
    osDelay(600);
}

void leg_gait(GaitServo *list,volatile uint8_t *mode,volatile uint8_t *want_mode)
{
	uint8_t k;
	float s;
	
	for (k = 0; k < GAIT_N; k++)
    {
		Level_Update();
		
		if ((*mode == MOVE) && (gait_hold == 0u))//里程计计算，累加，发送至上位机之后清零
        {
            Odom_Add(gait_v * ODOM_K_STRIDE / (float)GAIT_N);
        }
		
		float w_now = 0.0f;
        s = (float)k / (float)GAIT_N;
		
		        /* ===== 停步判断 =====
         * 注意：原地转弯（gait_v == 0 但 gait_turn != 0）也是"走路"。
         *       这里曾经只判 gait_v == 0，导致开环左右转一步都不迈。 */
        if ((*mode != STAND) && (gait_pid_en != 0u) && (gait_v == 0.0f))
        {
            /* 闭环停止：误差小就停步，偏多了就自动纠正 */
            float err = Yaw - gait_yaw_target;

            while (err >  180.0f) err -= 360.0f;
            while (err < -180.0f) err += 360.0f;

            if (gait_hold == 0u)
            {
                if ((err > -HOLD_ENTER_DEG) && (err < HOLD_ENTER_DEG))
                {
                    gait_hold = 1u;
                }
            }
            else if ((err < -HOLD_EXIT_DEG) || (err > HOLD_EXIT_DEG))
            {
                gait_hold = 0u;
            }
        }
        else if ((*mode != STAND) && (gait_pid_en == 0u) &&
                 (gait_v == 0.0f) && (gait_turn == 0.0f))
        {
            /* 开环，且既不前进也不转弯 → 停步 */
            gait_hold = 1u;
        }
        else
        {
            gait_hold = 0u;        /* 有任何运动意图，正常迈步 */
        }

        /* ===== 停车流程 ===== */

        /* ① 在 d≈0 的安全点踩刹车：步幅锁 0，脚就地垂直落下 */
        if (gait_stop_req && (gait_stopping == 0u) && ((k == 2u) || (k == (GAIT_N / 2u + 2u))))
        {
            gait_v        = 0.0f;
            gait_turn     = 0.0f;
            gait_stopping = 1u;
            gait_stop_req = 0u;
			
			Zhuanwan_PID.Integral   = 0.0f;
            Zhuanwan_PID.LastError  = 0.0f;
            Zhuanwan_PID.Output_Val = 0.0f;
        }

        /* ② 再走 3 拍，等还在空中那组腿落地， */
        if ((gait_stopping != 0u) && ((k == 5u) || (k == (GAIT_N / 2u + 3u))))
        {
            //*mode         = STAND;
            //*want_mode    = STAND;
            gait_stopping = 0u;
        }
		
        /* ===== 正常模式切换（停车中不响应）===== */
        if ((gait_stopping == 0u) && ((k == 2u) || (k == (GAIT_N / 2u + 3u))) && (*want_mode != *mode))
        {
            *mode = *want_mode;
        }

        /* ===== 航向环（停车中不介入，免得把 gait_turn 拉回去）===== */
        if ((*mode != STAND) && (gait_pid_en != 0u) && (gait_stopping == 0u))
        {
            /* 航向环每拍都算（微分连续），但只在 d≈0 处生效，避免脚突变 */
            w_now = YawPid();

            if ((k == 2u) || (k == (GAIT_N / 2u + 2u)))
            {
                gait_turn = w_now;
            }
        }

        /* 左侧：1~9 号舵机 */
        GaitPose(list, 0, s, *mode);
        GaitSend(list, 9, GAIT_T_FRAME-15);
        osDelay(20);

        /* 右侧：10~18 号舵机 */
        GaitPose(list, 3, s, *mode);
        GaitSend(list, 9, GAIT_T_FRAME-15);

        /* 【喘息时间 —— 不要删】 */
        osDelay(GAIT_DWELL);
    }	
}
