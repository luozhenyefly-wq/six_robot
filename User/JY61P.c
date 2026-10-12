#include "JY61P.h"
#include "usart.h"     
#include "bt_control.h"

/*延时方式：用 HAL_Delay，这样在 main() 里、RTOS 启动之前也能正常调用
  （参考工程正是在 main() 里紧跟初始化之后调 Serial_JY61P_Zero_Yaw() 的）
  注意：不要在中断服务函数里调本延时，会卡死*/
#define JY61P_DELAY_MS(x)    HAL_Delay(x)

/* ==================== 接收解析 ==================== */

/*串口接收状态标识*/
#define WAIT_HEADER1    0
#define WAIT_HEADER2    1
#define RECEIVE_DATA    2

/*帧类型：0x53 = 角度帧，0x52 = 陀螺仪（角速度）帧*/
#define FRAME_ANGLE     0x53
#define FRAME_GYRO      0x52

/*姿态角，单位：度（外部通过 extern 读取）*/
volatile float Pitch, Roll, Yaw;

/*Z 轴角速度（偏航角速度），单位：度/秒，正负号与 Yaw 一致*/
volatile float vYaw;

/*接收状态机*/
static uint8_t RxState    = WAIT_HEADER1;
static uint8_t frameType  = FRAME_ANGLE;	/*当前正在接收的帧类型*/
static uint8_t receivedData[9];		/*一帧的 8 个数据字节 + 1 个校验和字节*/
static uint8_t dataIndex  = 0;

/*单字节接收缓存（HAL 中断接收用）*/
static uint8_t rxByte;

extern volatile uint8_t bt_go_flag;

/**
  * 函    数：把 16 位原始角度值换算成 -180~180 度（内部使用）
  * 参    数：Low 低字节，High 高字节
  * 返 回 值：角度，单位：度，范围 -180~180
  */
static float JY61P_ToAngle(uint8_t Low, uint8_t High)
{
	float angle = (float)(((uint16_t)High << 8) | Low) / 32768.0f * 180.0f;

	/*超过 180 度，说明原始值最高位为 1，代表负角度*/
	if (angle > 180.0f)
	{
		angle -= 360.0f;
	}
	return angle;
}

/**
  * 函    数：把 16 位原始角速度值换算成 -2000~2000 度/秒（内部使用）
  * 参    数：Low 低字节，High 高字节
  * 返 回 值：角速度，单位：度/秒
  * 说    明：JY61P 陀螺仪量程固定为 ±2000°/s，原始值是有符号 16 位，
  *           32768 对应满量程 2000°/s，所以直接按 int16_t 解读再乘系数。
  */
static float JY61P_ToGyro(uint8_t Low, uint8_t High)
{
	int16_t raw = (int16_t)(((uint16_t)High << 8) | Low);

	return (float)raw / 32768.0f * 2000.0f;
}

/**
  * 函    数：解析收到的一个字节（内部使用）
  * 参    数：uartdata 刚收到的字节
  * 返 回 值：无
  * 说    明：JY61P 每帧固定 11 字节：0x55 + 帧类型 + 8 个数据字节 + 1 个校验和。
  *           下面是【两种各自独立的帧】，不是一帧分两行写；两种帧长度一样，
  *           所以共用同一个状态机，靠 frameType 区分数据含义。末尾两个数据字节
  *           在两种帧里含义完全不同：0x53 是版本号，0x52 是温度。
  *
  *             角度帧 0x53（11 字节）
  *               0x55 | 0x53 | RollL RollH | PitchL PitchH | YawL YawH | VL VH | SUM
  *               包头 | 类型 |  data[0..1] |  data[2..3]  | data[4..5] | 版本号 | 校验
  *               -> 更新 Roll / Pitch / Yaw
  *
  *             角速度帧 0x52（11 字节）
  *               0x55 | 0x52 |  WxL  WxH  |  WyL  WyH  | WzL WzH | TL TH | SUM
  *               包头 | 类型 |  data[0..1] | data[2..3] | data[4..5] | 温度 | 校验
  *               -> 取 data[4..5]（Wz）更新 vYaw
  *
  *           两种帧的 data[6..7]（版本号 / 温度）本工程都不使用，但必须参与校验和计算。
  */
static void JY61P_ParseByte(uint8_t uartdata)
{
	switch (RxState)
	{
	case WAIT_HEADER1:
		if (uartdata == 0x55)
		{
			RxState = WAIT_HEADER2;
		}
		break;

	case WAIT_HEADER2:
		if (uartdata == FRAME_ANGLE || uartdata == FRAME_GYRO)
		{
			frameType = uartdata;			/*记住本帧类型，校验和计算时要用*/
			RxState = RECEIVE_DATA;
			dataIndex = 0;
		}
		else if (uartdata == 0x55)
		{
			RxState = WAIT_HEADER2;			/*这一字节本身就是新帧的第一个头，继续等第二个头*/
		}
		else
		{
			RxState = WAIT_HEADER1;			/*不是期望的第二个头，重置状态*/
		}
		break;

	case RECEIVE_DATA:
		receivedData[dataIndex] = uartdata;
		dataIndex ++;
		if (dataIndex == 9)			/*8 个数据字节收齐了，第 9 个是校验和*/
		{
			/*校验和 = 0x55 + 帧类型 + 前 8 个数据字节之和（0x52 / 0x53 都是这个算法）*/
			uint8_t calculatedSum = (uint8_t)(0x55 + frameType);

			for (uint8_t i = 0; i < 8; i++)
			{
				calculatedSum = (uint8_t)(calculatedSum + receivedData[i]);
			}

			if (calculatedSum == receivedData[8])
			{
				if (frameType == FRAME_ANGLE)
				{
					/*0x53 角度帧：data[0..1]=Roll，data[2..3]=Pitch，data[4..5]=Yaw，
					  data[6..7]=版本号 VL VH（和角度在同一个包里，本工程用不到）*/
//					Roll  = JY61P_ToAngle(receivedData[0], receivedData[1]);
//					Pitch = JY61P_ToAngle(receivedData[2], receivedData[3]);
//					Yaw   = JY61P_ToAngle(receivedData[4], receivedData[5]);
					
					if (frameType == FRAME_ANGLE)
					{
						/* 协议原始顺序：data[0..1]=Roll，data[2..3]=Pitch，data[4..5]=Yaw */
						float imu_roll  = JY61P_ToAngle(receivedData[0], receivedData[1]);
						float imu_pitch = JY61P_ToAngle(receivedData[2], receivedData[3]);

						/* ---- 安装姿态补偿 ----
						 * 本车 JY61P 绕竖直轴装了 90°（实测）：
						 *     机头抬起 → 模块 Roll  增大
						 *     左侧抬起 → 模块 Pitch 增大
						 * 所以交叉一次：模块 Roll 给机身俯仰，模块 Pitch 给机身横滚。*/
						Pitch = imu_roll;
						Roll  = imu_pitch;

						Yaw   = JY61P_ToAngle(receivedData[4], receivedData[5]);
					}
				}
				else
				{
					/*0x52 角速度帧：data[0..1]=Wx，data[2..3]=Wy，data[4..5]=Wz，
					  data[6..7]=温度 T（本工程用不到）*/
					vYaw  = JY61P_ToGyro(receivedData[4], receivedData[5]);
				}
			}
			else
			{
				/*校验失败，丢弃本帧（保留上一次的有效值）*/
			}

			RxState = WAIT_HEADER1;		/*重置状态以等待下一个数据包*/
		}
		break;

	default:
		RxState = WAIT_HEADER1;
		break;
	}
}

/* ==================== 对外接口 ==================== */

/**
  * 函    数：JY61P 初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：启动串口中断接收，必须在 MX_UART4_Init() 之后调用
  */
void JY61P_Init(void)
{
	RxState   = WAIT_HEADER1;
	frameType = FRAME_ANGLE;
	dataIndex = 0;
	vYaw      = 0.0f;

	/*启动第一次中断接收，收满 1 个字节后进入 HAL_UART_RxCpltCallback*/
	HAL_UART_Receive_IT(&huart4, &rxByte, 1);
}

/**
  * 函    数：串口接收完成回调（重写 HAL 的弱函数）
  * 参    数：huart 触发回调的串口句柄
  * 返 回 值：无
  * 说    明：每收到 1 个字节解析一次，然后重新武装接收，形成逐字节接收。
  *           注意：工程里其他地方若也写了 HAL_UART_RxCpltCallback，必须合并成同一个函数，
  *                 否则会因函数重名导致链接失败。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART4)
	{
		JY61P_ParseByte(rxByte);

		/*重新武装，继续收下一个字节*/
		HAL_UART_Receive_IT(&huart4, &rxByte, 1);
	}
	else if (huart->Instance == USART3)          /* 蓝牙分支 */
	{	
		Bt_OnRxComplete();
	}
}

/**
  * 函    数：串口错误回调（重写 HAL 的弱函数）
  * 参    数：huart 触发回调的串口句柄
  * 返 回 值：无
  * 说    明：串口发生溢出/帧错误时 HAL 会自动停止中断接收，这里重新武装，
  *           避免接收永久停摆（FreeRTOS 下任务切换可能导致短暂来不及取数据）
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART4)
	{
		__HAL_UART_CLEAR_OREFLAG(huart);			/*清溢出标志*/

		RxState = WAIT_HEADER1;						/*帧已错位，重新同步*/
		HAL_UART_Receive_IT(&huart4, &rxByte, 1);	/*重新武装接收*/
	}
	else if (huart->Instance == USART3)          
	{
		Bt_OnError();
	}
}

/**
  * 函    数：发送偏航角置零命令
  * 参    数：无
  * 返 回 值：无
  * 说    明：解锁 -> Z 轴角度置零 -> 保存，每步之间延时 100ms
  */
void Serial_JY61P_Zero_Yaw(void)
{
	static const uint8_t cmd1[5] = {0xFF, 0xAA, 0x69, 0x88, 0xB5};	/*解锁*/
	static const uint8_t cmd2[5] = {0xFF, 0xAA, 0x01, 0x04, 0x00};	/*Z 轴角度置零*/
	static const uint8_t cmd3[5] = {0xFF, 0xAA, 0x00, 0x00, 0x00};	/*保存*/

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd1, 5, 100);
	JY61P_DELAY_MS(100);

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd2, 5, 100);
	JY61P_DELAY_MS(100);

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd3, 5, 100);
}

/**
  * 函    数：让模块输出陀螺仪（角速度）帧 0x52（可选）
  * 参    数：无
  * 返 回 值：无
  * 说    明：JY61P 出厂默认就把加速度 0x51、角速度 0x52、角度 0x53 三帧都发出来，
  *           正常情况不用调本函数。只有当 vYaw 一直是 0、而 Yaw 正常变化时，
  *           说明模块的输出内容被改过（没在发 0x52 帧），此时在 JY61P_Init() 之后
  *           调一次本函数即可（写寄存器 0x02 = 0x07：三帧全开，然后保存）。
  *           整个函数阻塞约 200ms，不要在中断里调用。
  */
void Serial_JY61P_Enable_Gyro_Output(void)
{
	static const uint8_t cmd1[5] = {0xFF, 0xAA, 0x69, 0x88, 0xB5};	/*解锁*/
	static const uint8_t cmd2[5] = {0xFF, 0xAA, 0x02, 0x07, 0x00};	/*输出内容 RSW=0x07*/
	static const uint8_t cmd3[5] = {0xFF, 0xAA, 0x00, 0x00, 0x00};	/*保存*/

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd1, 5, 100);
	JY61P_DELAY_MS(100);

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd2, 5, 100);
	JY61P_DELAY_MS(100);

	HAL_UART_Transmit(&huart4, (uint8_t *)cmd3, 5, 100);
}
