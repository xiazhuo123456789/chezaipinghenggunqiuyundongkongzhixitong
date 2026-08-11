/*
 * pages.h — 所有页面/菜单/显示统一接口
 *
 * 合并: menu + page2/3/4/5 + display
 *
 * 页面布局:
 *   1 — Hello World
 *   2 — 循迹 (KEY2=启动, KEY3=停车)
 *   3 — 舵机测试 (KEY2=+5°, KEY3=-5°)
 *   4 — 闭环摆杆任务 (KEY2=启动, KEY3=复位, 位置→判稳→转移)
 *   5 — 视觉跟随 (KEY2=启动, KEY3=停车, TIMG7 50Hz PID)
 *
 * 调用约定 (main 里每轮循环):
 *   Pages_HandleKey(key);   // 按键分发 (第一优先级)
 *   Pages_RunActivePage();  // 当前页面活跃逻辑 (第二优先级)
 *
 * 注意: 各页面内部已自行做显示刷新, 不再需要全局 DisplayUpdate。
 *
 * 视觉PID 移植自 Keil 省赛 Q3_Servo_Control_New (pid.c:540-605)
 *   - 速度估计 (一阶低通)
 *   - 保守 PID (Kp=0.12 Kd=0.36 Ki=0.002)
 *   - 死区 (双条件: dx≤5 且 vel<1)
 *   - 超时保护 (K230 断连 5000ms → 自动回 90°)
 *   - 判稳计数 (主循环用 stay_cnt 决定状态转移)
 */

#ifndef __PAGES_H
#define __PAGES_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 视觉闭环全局变量 (主循环+中断共享)
 * ================================================================ */
extern volatile int16_t g_vision_target_pos;  /* 钢球目标位置 (像素, 相对中心) */
extern volatile int16_t g_vision_ball_vel;    /* 钢球速度估计 (定标 x100) */
extern volatile uint16_t g_vision_stay_cnt;   /* 判稳计数 (50Hz, 1=20ms) */
extern volatile uint8_t  g_vision_timeout;    /* K230 超时标志: 1=断连 */

/* ================================================================
 * 统一 API — main() 只调这 2 个
 * ================================================================ */
void Pages_HandleKey(uint8_t key);    /* 按键事件分发 */
void Pages_RunActivePage(void);       /* 当前页面的活跃逻辑 */
/* 注意: 各页面内部已自行做显示刷新, 不再需要全局 DisplayUpdate */

/* ================================================================
 * 页面 5 专用 TIMG7 定时器初始化 (由 Board_Init 调用)
 * ================================================================ */
void Page5_TimerInit(void);           /* TIMG7 50Hz 视觉 PID 中断 */

/* 视觉 PID 计算 (主文件 TIMG7_IRQHandler 调用) */
void Vision_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __PAGES_H */
