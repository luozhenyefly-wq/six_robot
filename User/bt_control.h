#ifndef __BT_CONTROL_H
#define __BT_CONTROL_H

#include "main.h"
#include "usart.h" 
#include "leg_gait.h"
#include "cmsis_os.h"
#include "PID.h"
#include "face.h"
/* ---- 运动参数 ---- */
#define BT_V_FWD       10.0f    //前进步幅 cm //12
#define BT_V_BACK     -10.0f    //后退步幅 cm 
#define BT_TURN         10.0f    //原地转弯步幅 cm //8

/* ---- 角度录入状态机 ---- */
#define BT_ST_IDLE     0u
#define BT_ST_SIGN     1u       //已收到 'A'，等符号或数字 
#define BT_ST_NUM      2u       //正在录数字 

#define BT_ANGLE_TIMEOUT_MS   100u
#define BT_POLL_TIMEOUT_MS   100u      /* 队列阻塞超时 = 角度录入超时 */

void Bt_Init(void);             /* 启动接收，在 main() 里调一次 */
void Bt_OnRxComplete(void);     /* 由 HAL_UART_RxCpltCallback 调用 */
void Bt_Poll(void);             /* 由 bt_control_Task 循环调用 */
void Bt_OnError(void);
#endif
