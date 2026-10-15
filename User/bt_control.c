#include "bt_control.h"


static uint8_t  btState    = BT_ST_IDLE;
static uint8_t  btNeg      = 0u;
static int16_t  btAngle    = 0;
static uint32_t btLastTick = 0u;

extern volatile float Pitch, Roll, Yaw;
extern volatile uint8_t gait_want_mode;//移动模式
extern volatile uint8_t bt_go_flag;
extern PID Zhuanwan_PID;
	
static uint8_t btRxByte = 0u;

static void Bt_Go(float v, float turn, uint8_t pid)
{
	if (bt_go_flag == 0)       //总开关没按下，运动指令不生效
    {
        return;
    }
	
	Zhuanwan_PID.Integral   = 0.0f;
    Zhuanwan_PID.LastError  = 0.0f;
    Zhuanwan_PID.Output_Val = 0.0f;
	
    gait_v         = v;
    gait_turn      = turn;
    gait_pid_en    = pid;
    gait_want_mode = MOVE;
}

static void Bt_ExecAngle(void)
{
    int16_t deg = btNeg ? (int16_t)(-btAngle) : btAngle;

    while (deg >  180) deg -= 360;      /* 归一到 ±180 */
    while (deg < -180) deg += 360;

    gait_yaw_target = (float)deg;
    Bt_Go(0.0f, 0.0f, 1u);              /* 原地、闭环转到目标 */

    btState = BT_ST_IDLE;
    btNeg   = 0u;
    btAngle = 0;
}

static void Bt_DropAngle(void)
{
    btState = BT_ST_IDLE;
    btNeg   = 0u;
    btAngle = 0;
}

static void Bt_HandleByte(uint8_t c)
{
    if (btState == BT_ST_SIGN)
    {
        if (c == '-')
        {
            btNeg      = 1u;
            btState    = BT_ST_NUM;
            btLastTick = HAL_GetTick();
        }
        else if (c >= '0' && c <= '9')
        {
            btAngle    = (int16_t)(c - '0');
            btState    = BT_ST_NUM;
            btLastTick = HAL_GetTick();
        }
        else
        {
            Bt_DropAngle();             /* 非法输入 */
        }
        return;
    }

    /* ---- 录数字 ---- */
    if (btState == BT_ST_NUM)
    {
        if (c >= '0' && c <= '9')
        {
            btAngle = (int16_t)(btAngle * 10 + (c - '0'));
            if (btAngle > 999)
            {
                btAngle = 999;
            }
            btLastTick = HAL_GetTick();
            return;
        }

        /* 遇到非数字，命令结束，立刻执行。这个字符本身不再解释 */
        Bt_ExecAngle();
        return;
    }

    /* ---- 空闲：单字符命令 ---- */
    switch (c)
    {
        case 'F': case 'f':
            Bt_Go(BT_V_FWD, 0.0f, 1u);
			gait_yaw_target = Yaw;
            break;

        case 'B': case 'b':
            Bt_Go(BT_V_BACK, 0.0f, 1u);
			gait_yaw_target = Yaw;
            break;

        case 'L': case 'l':
            Bt_Go(0.0f, -BT_TURN, 0u);      // 开环持续左转
            break;

        case 'R': case 'r':
            Bt_Go(0.0f, BT_TURN, 0u);       // 开环持续右转
            break;

        case 'S': case 's':
			gait_pid_en   = 0u;
            gait_stop_req = 1u;             // 开环停止
            break;

		case 'H': case 'h':                 // 闭环停止：停下但守住航向 
            gait_pid_en   = 1u;
            gait_stop_req = 1u;
            break;
		
        case 'A': case 'a':				   //转到目标角度（绝对角度）
            btState    = BT_ST_SIGN;
            btNeg      = 0u;
            btAngle    = 0;
            btLastTick = HAL_GetTick();
            break;
		
		case '1': Face_SetEmotion(FACE_NORMAL);   break;
        case '2': Face_SetEmotion(FACE_HAPPY);    break;
        case '3': Face_SetEmotion(FACE_SAD);      break;
        case '4': Face_SetEmotion(FACE_ANGRY);    break;
        case '5': Face_SetEmotion(FACE_SURPRISE); break;
        case '6': Face_SetEmotion(FACE_SLEEP);    break;
		case '7': Face_SetEmotion(FACE_SHAZI);    break;
		
        default:
            break;                          // 忽略 \r \n 空格等 
    }
}


void Bt_Init(void)
{
    HAL_UART_Receive_IT(&huart3, &btRxByte, 1);     // 启动首次接收 
}

void Bt_OnRxComplete(void)
{
	//将收到的消息放进消息队列中
	osMessageQueuePut(bt_msg_QueueHandle, &btRxByte, 0, 0); 
	HAL_UART_Receive_IT(&huart3, &btRxByte, 1);
}

void Bt_Poll(void)
{
     uint8_t c;

    /* 阻塞等一个字节，最多等 100ms */
    if (osMessageQueueGet(bt_msg_QueueHandle, &c, NULL, BT_POLL_TIMEOUT_MS) == osOK)
    {
        Bt_HandleByte(c);
        return;
    }

/* 只有"100ms 没等到数据"才会走到这里 —— 顺便当角度录入的超时检查 */
    if ((btState != BT_ST_IDLE) &&
        ((HAL_GetTick() - btLastTick) > BT_ANGLE_TIMEOUT_MS))
    {
        if (btState == BT_ST_NUM)
        {
            Bt_ExecAngle();
        }
        else
        {
            Bt_DropAngle();
        }
    }
}
void Bt_OnError(void)
{
	__HAL_UART_CLEAR_OREFLAG(&huart3);
	HAL_UART_Receive_IT(&huart3, &btRxByte, 1);
}
