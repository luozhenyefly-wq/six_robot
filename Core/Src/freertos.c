/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "JY61P.h"
#include "tim.h"
#include "pid.h"
#include "leg_angle.h"
#include "usart.h"
#include <stdint.h>
#include "leg_gait.h"
#include "key.h"
#include "bt_control.h"
#include "face.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern volatile float Pitch, Roll, Yaw;
extern volatile float vYaw;
extern volatile uint16_t count1;
extern char A,B,C,D;
extern LegMount LEG1,LEG2,LEG3,LEG4,LEG5,LEG6;
extern volatile uint8_t gait_mode;
extern volatile uint8_t gait_want_mode;//移动模式
extern volatile uint8_t gait_stop_req; 
extern volatile uint8_t go_flag;
extern volatile int e_flag;
extern volatile uint8_t bt_go_flag;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for yaw_send */
osThreadId_t yaw_sendHandle;
const osThreadAttr_t yaw_send_attributes = {
  .name = "yaw_send",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow5,
};
/* Definitions for leg_gait */
osThreadId_t leg_gaitHandle;
const osThreadAttr_t leg_gait_attributes = {
  .name = "leg_gait",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow6,
};
/* Definitions for key */
osThreadId_t keyHandle;
const osThreadAttr_t key_attributes = {
  .name = "key",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow2,
};
/* Definitions for bt_control */
osThreadId_t bt_controlHandle;
const osThreadAttr_t bt_control_attributes = {
  .name = "bt_control",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow4,
};
/* Definitions for bt_msg_Queue */
osMessageQueueId_t bt_msg_QueueHandle;
const osMessageQueueAttr_t bt_msg_Queue_attributes = {
  .name = "bt_msg_Queue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void yaw_send_Task(void *argument);
void leg_gait_Task(void *argument);
void key_Task(void *argument);
void bt_control_Task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of bt_msg_Queue */
  bt_msg_QueueHandle = osMessageQueueNew (16, sizeof(uint8_t), &bt_msg_Queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of yaw_send */
  yaw_sendHandle = osThreadNew(yaw_send_Task, NULL, &yaw_send_attributes);

  /* creation of leg_gait */
  leg_gaitHandle = osThreadNew(leg_gait_Task, NULL, &leg_gait_attributes);

  /* creation of key */
  keyHandle = osThreadNew(key_Task, NULL, &key_attributes);

  /* creation of bt_control */
  bt_controlHandle = osThreadNew(bt_control_Task, NULL, &bt_control_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
	//OLED_ShowString(10, 10,"haha",OLED_6X8);
	OLED_Printf(10, 10,OLED_6X8, "bt_go_flag:%d",bt_go_flag);
	OLED_Printf(10, 25,OLED_6X8, "pitch:%5.2f",Pitch);
	OLED_Printf(10, 40,OLED_6X8, "Roll:%6.1f",Roll);   /*陀螺仪 Z 轴角速度，单位 度/秒，用来确认 0x52 帧收到了*/
	//OLED_ShowFloatNum(10, 40,count1,4,3,OLED_6X8);
	OLED_Update();
  osDelay(70);
//	Face_Update();
//    osDelay(40);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_yaw_send_Task */
/**
* @brief Function implementing the yaw_send thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_yaw_send_Task */
void yaw_send_Task(void *argument)
{
  /* USER CODE BEGIN yaw_send_Task */
  /* Infinite loop */
  for(;;)
  {
	/*
	  负责将机器yaw,vyaw,odom发送至上位机，如不需要slam和导航可清空该部分
	*/
	float ds = Odom_Take(); 
	  
	int16_t t_Yaw=(int16_t)(Yaw*100);
	float tmp_vYaw = vYaw * 100.0f;
	float   tmp_d  = ds   * 100.0f;
	 
	//防止溢出
	if(tmp_vYaw >  32767.0f) tmp_vYaw =  32767.0f;
	if(tmp_vYaw < -32768.0f) tmp_vYaw = -32768.0f;
	if(tmp_d >  32767.0f) tmp_d =  32767.0f;
    if(tmp_d < -32768.0f) tmp_d = -32768.0f;
	  
	int16_t t_vYaw=(int16_t)tmp_vYaw;
	int16_t t_ds   = (int16_t)tmp_d;
	  
	uint8_t a[8];
	a[0] = 0xAA;
	a[1] = (t_Yaw >> 8) & 0xFF;  // 取高8位
	a[2] = t_Yaw & 0xFF;         // 取低8位
	a[3] = (t_vYaw >> 8) & 0xFF; // 同上
	a[4] = t_vYaw & 0xFF;         
	a[5] = (t_ds   >> 8) & 0xFF; //同上
    a[6] =  t_ds  & 0xFF;
	a[7] = 0xBB;
	
	HAL_UART_Transmit(&huart6,a,8,100);  
	osDelay(20);
  }
  /* USER CODE END yaw_send_Task */
}

/* USER CODE BEGIN Header_leg_gait_Task */
/**
* @brief Function implementing the leg_gait thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_leg_gait_Task */
void leg_gait_Task(void *argument)
{
  /* USER CODE BEGIN leg_gait_Task */
	
	/*步态控制任务*/
	Serial_JY61P_Zero_Yaw();   
	GaitServo list[9];
	gait_init(list,&gait_want_mode);
	
  /* Infinite loop */
  for(;;)
  {
	leg_gait(list,&gait_mode,&gait_want_mode);
    osDelay(1);
  }
  /* USER CODE END leg_gait_Task */
}

/* USER CODE BEGIN Header_key_Task */
/**
* @brief Function implementing the key thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_key_Task */
void key_Task(void *argument)
{
  /* USER CODE BEGIN key_Task */
  /* Infinite loop */
  for(;;)
  {
	robot_go_key();
    osDelay(20);
  }
  /* USER CODE END key_Task */
}

/* USER CODE BEGIN Header_bt_control_Task */
/**
* @brief Function implementing the bt_control thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_bt_control_Task */
void bt_control_Task(void *argument)
{
  /* USER CODE BEGIN bt_control_Task */
  /* Infinite loop */
  for(;;)
  {
    Bt_Poll();          /* 解析缓冲里的命令 + 角度录入超时检查 */
    osDelay(20);
  }
  /* USER CODE END bt_control_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

