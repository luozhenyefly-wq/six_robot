/* 
  本工程使用的是24路舵机控制板，通过上位机串口发送数据控制舵机
  具体通信协议需要参考不同商家的手册
  ============================================================================
  leg.c —— 24 路舵机控制器 / 总线舵机（从串口 5 发送）
 
  用 sprintf 拼指令，格式串和手册一一对应：
      #004P1500T0800!
       #  004   P  1500   T  0800   !
       ↑ 3位ID  ↑ 4位PWM  ↑ 4位ms  ↑ 末尾必须有 '!'
 
  固定 15 字节 —— 位数不足要用 0 补齐，所以格式串写成 %03u / %04u。
  用法：main 里 MX_UART5_UART_Init(); 之后  Servo_MoveDeg(4, 90, 800);
  ===========================================================================
*/

#include "leg.h"

extern UART_HandleTypeDef huart5;

/* 
   把拼好的指令从串口 5 发出去。len 是字符个数（sprintf 的返回值）。
   整个文件只有这里需要类型转换：指令是 char*，而 HAL 要 uint8_t* 
 */
static void Send(const char *cmd, int len)
{
    HAL_UART_Transmit(&huart5, (uint8_t *)cmd, (uint16_t)len, 100);
}

/* id 号舵机在 ms 毫秒内转到 pwm 位置（pwm 500~2500，1500 是中位） */
void Servo_Move(uint8_t id, uint16_t pwm, uint16_t ms)
{
    char cmd[48];                          /* 15 字节指令 + 字符串结束符 */
    int  n;

    if (pwm < 500)  pwm = 500;             /* 控制器只认 500~2500 */
    if (pwm > 2500) pwm = 2500;
	
    if (ms  > 9999) ms  = 9999;            /* ms 只有 4 位 */

    /* %03u = 至少 3 位、不足补 0；%04u = 至少 4 位、不足补 0
     * sprintf 的返回值 n = 写了几个字符，这里恒为 15 */
    n = sprintf(cmd, "#%03uP%04uT%04u!",(unsigned)id, (unsigned)pwm, (unsigned)ms);
							
    Send(cmd, n);                         
}

/* 按角度控制：deg 是 0~SERVO_MAX_DEG（在 Servo.h 里设，270° 舵机就是 0~270） */
void Servo_MoveDeg(uint8_t id, uint16_t deg,uint16_t ms)
{
    if (deg > SERVO_MAX_DEG) deg = SERVO_MAX_DEG;
    Servo_Move(id, SERVO_PWM(deg),ms);
}


/* 停在当前位置（是停住不动，不是断电），用来打断动作：发 $DST:4! */
void Servo_Stop(uint8_t id)
{
    char cmd[12];
    int  n = sprintf(cmd, "$DST:%u!", (unsigned)id);

    Send(cmd, n);                          /* 这里的长度是可变的，用返回值最省事 */
}

/* ---------------------------------------------------------------------------
 * 兜底接口：别的指令直接发字符串（手册附录1 的指令都能用）
 *     Servo_SendCmd("$DST!");          // 所有舵机停住
 *     Servo_SendCmd("$RST!");          // 控制器复位
 *     Servo_SendCmd("$DGT:0-4,1!");    // 调用已下载的动作组 G0000~G0004，1 遍
 *     Servo_SendCmd("{#000P1000T0800!#001P1833T0800!}");   // 两个舵机同时到位
 * -------------------------------------------------------------------------*/
void Servo_SendCmd(const char *cmd)
{
    Send(cmd, (int)strlen(cmd));
}
