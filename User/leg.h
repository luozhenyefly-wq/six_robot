/* ============================================================================
 * Servo.h —— 24 路舵机控制器 / 总线舵机 串口控制（STM32 HAL，用串口 5 发送）
 *
 * 四个函数：
 *     Servo_MoveDeg(4, 90, 800);    // 4 号舵机转到 90°，用 800ms
 *     Servo_Move(4, 1500, 800);     // 同上，直接给 PWM 值
 *     Servo_Stop(4);                // 4 号舵机停在当前位置
 *     Servo_SendCmd("$DGT:0-4,1!"); // 兜底：直接发任意指令
 *
 * 角度和 PWM 的对应：
 *      PWM       500    1000     1500     2000     2500
 *      180°舵机    0°     45°      90°     135°     180°
 *      270°舵机    0°    67.5°    135°    202.5°    270°   
 * 也就是说 270° 舵机的"中位"1500 是 135°，不是 90°。
 *
 * CubeMX：UART5 选 Asynchronous，115200 / 8 位 / 无校验 / 1 位停止
 *         （F103 上 UART5 是 TX=PC12、RX=PD2，只发也要生成一下）
 * 接线：  STM32 PC12(TX) → 控制器 RX，STM32 GND ? 控制器 GND
 *         控制器自己用电源端子供电，不要靠串口供电
 * ===========================================================================*/

#ifndef LEG_H
#define LEG_H

#include "main.h"
#include <stdio.h>      
#include <string.h>     

/* 舵机行程角：本工程所使用舵机是 270°，就填 270；如果是 180° 舵机改成 180 */
#define SERVO_MAX_DEG    270

/* 角度换 PWM：0°→500，中位→1500，满行程→2500
    270° 舵机：1500 = 135°
    180° 舵机：1500 = 90°
  下面这个式子跟着 SERVO_MAX_DEG 自动变 
*/
#define SERVO_PWM(deg)   (500 + (deg) * 2000 / SERVO_MAX_DEG)

void Servo_Move(uint8_t id, uint16_t pwm, uint16_t ms);    /* 直接给 PWM 500~2500 */
void Servo_MoveDeg(uint8_t id, uint16_t deg,uint16_t ms);      /* 给角度 0~SERVO_MAX_DEG */
void Servo_Stop(uint8_t id);                                       /* 停在当前位置 */
void Servo_SendCmd(const char *cmd);                               /* 兜底：发任意指令 */

#endif
