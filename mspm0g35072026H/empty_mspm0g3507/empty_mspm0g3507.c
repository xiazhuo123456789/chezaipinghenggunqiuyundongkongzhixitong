/*
 * empty_mspm0g3507.c — MSPM0G3507 主程序 (江协科技架构)
 *
 * 架构: 三中断 (TIMG6+TIMG7+UART0) + 主循环
 *   TIMG6 10ms: 系统时钟 + 编码器同步 + 循迹控制 + 按键扫描 + 任务协调
 *   TIMG7 50Hz: 稳球序列状态机 + 视觉 PID 计算 → 舵机
 *   UART0:      K230 视觉 dx 接收
 *   主循环:     Pages_HandleKey → Pages_RunActivePage
 *
 * 页面: 1=Hello  2=循迹(T2)  3=舵机  4=钢球(T3)  5=视觉(T4)
 */
#include "ti_msp_dl_config.h"
#include "init.h"
#include "encoder.h"
#include "tracking.h"
#include "key.h"
#include "oled.h"
#include "uart.h"
#include "pages.h"
#include "task_manager.h"

void __mpu_init(void) {}
void _system_post_cinit(void) {}

int main(void)
{
    Board_Init();
    OLED_ShowPage(1);

    for (;;)
    {
        Pages_HandleKey(Key_GetNum());
        Pages_RunActivePage();
    }
}

/************************ 中断 **************************/

void TIMG6_IRQHandler(void)
{
    g_uptime_ms += 10;
    Encoder_Sync();
    Tracking_Tick();
    Key_Tick();
    Task_Tick();                    /* 任务协调 (停车→稳球切换等) */
    DL_TimerG_clearInterruptStatus(ENC_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
}

extern void Vision_Seq_Tick(void);

void TIMG7_IRQHandler(void)
{
    Vision_Seq_Tick();              /* 稳球序列状态机 */
    Vision_Tick();                  /* 视觉 PID 计算 + 舵机输出 */
    DL_TimerG_clearInterruptStatus(TIMG7, DL_TIMER_INTERRUPT_ZERO_EVENT);
}

void UART0_IRQHandler(void)
{
    UART_Tick();
}
