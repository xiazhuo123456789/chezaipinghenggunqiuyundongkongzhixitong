/*
 * servo.h — 舵机 PWM API (MSPM0G3507)
 *
 * 引脚宏已迁移到 init.h
 * 引脚: PA8 → TIMA0_CCP0 (IOMUX_PINCM19, PF=5)
 * 定时器: TIMA0, 50Hz (20ms 周期)
 * 脉冲范围: 0.5ms (0°) ~ 2.5ms (180°)
 *
 * 高精度模式:
 *   时钟: 32MHz / 10 = 3.2MHz (312.5ns/tick)
 *   周期: 63999 — 刚好填满 16 位
 *   分辨率: ~0.028°/tick (每度约 35.56 tick)
 */

#ifndef __SERVO_H
#define __SERVO_H

#include "init.h"   /* 引脚宏来自 init.h */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * API
 * ================================================================ */

/* 初始化舵机 PWM (TIMA0 CCP0 → PA8, 50Hz, 初始 90°) */
void Servo_PWM_Init(void);

/* 设置角度 (0°~180°, 精度 ~0.028°) */
void Servo_Set_Angle(uint16_t angle);

/* 设置角度 (浮点版, 支持小数度, 如 81.5°) */
void Servo_Set_AngleF(float angle);

/* 原始脉冲写入 (1600~8000 对应 0.5ms~2.5ms) */
void Servo_Set_Pulse(uint16_t cc_val);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
