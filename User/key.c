#include "key.h"

volatile uint8_t bt_go_flag;
extern volatile uint8_t gait_want_mode;
extern volatile float Pitch, Roll, Yaw;

void robot_go_key(void)//PB6，按下后机器人才会开始接收命令移动
{
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == 0) 
    {
        osDelay(10); 
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == 0) 
        {  
            while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == 0) 
            {
                osDelay(10); 
            }   
			bt_go_flag = 1;
        }   
    }
}
