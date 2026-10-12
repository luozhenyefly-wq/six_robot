#ifndef __JY61P_H
#define __JY61P_H

#include "main.h"

/*姿态解算结果，单位：度*/
/*这几个变量在串口接收回调（中断上下文）里被更新，在任务里被读取，*/
/*必须加 volatile，否则编译器可能把它们缓存进寄存器甚至优化掉*/
extern volatile float Pitch, Roll, Yaw;

/*Z 轴角速度（偏航角速度），单位：度/秒，来自模块的 0x52 角速度帧*/
/*正值表示 Yaw 正在增大，静止时受零漂影响会有 ±0.1°/s 左右的小波动*/
extern volatile float vYaw;

/**
  * 函    数：JY61P 初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：启动串口中断接收，必须在 MX_UART4_Init() 之后调用。
  *           建议在 main() 里 osKernelStart() 之前调用一次
  */
void JY61P_Init(void);

/**
  * 函    数：发送偏航角置零命令
  * 参    数：无
  * 返 回 值：无
  * 说    明：解锁 -> Z 轴角度置零 -> 保存，每步之间延时 100ms，
  *           整个函数会阻塞约 200ms。
  *           参考工程是在 main() 里初始化完成后立刻调一次，这样 Yaw 就从 0 开始。
  *           注意：不要在中断服务函数里调用（内部有延时，会卡死）
  */
void Serial_JY61P_Zero_Yaw(void);

/**
  * 函    数：让模块输出陀螺仪（角速度）帧 0x52（一般不需要调用）
  * 参    数：无
  * 返 回 值：无
  * 说    明：出厂默认三帧（0x51/0x52/0x53）全开，不需要调。
  *           只有当 Yaw 正常变化、而 vYaw 恒为 0 时，才在 JY61P_Init() 之后调一次。
  *           阻塞约 200ms，不要在中断里调用。
  */
void Serial_JY61P_Enable_Gyro_Output(void);

#endif
