/*
 * init.c — 全板统一初始化
 */
#include "init.h"
#include "encoder.h"
#include "tracking.h"
#include "oled.h"
#include "key.h"
#include "servo.h"
#include "uart.h"
#include "pages.h"          /* Page5_TimerInit */
#include "task_manager.h"   /* Task_Stop */

void Board_Init(void)
{
    SYSCFG_DL_init();
    Motor_PWM_Init();
    Encoder_Init();
    Tracking_Init();
    OLED_Init();
    Key_Init();
    Servo_PWM_Init();
    UART_Init();
    Page5_TimerInit();      /* TIMG7 50Hz, 必须在 UART/Servo 之后 */
    Task_Stop();            /* 确保所有任务初始为停止 */
}
