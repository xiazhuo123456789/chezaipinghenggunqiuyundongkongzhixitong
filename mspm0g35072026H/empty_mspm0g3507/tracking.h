/*
 * tracking.h — 5路红外寻迹模块 API
 * MSPM0G3507 (天猛星 3507)
 *
 * 引脚宏已迁移到 init.h
 *
 * 传感器排列 (俯视, 车头朝前):
 *   [0]     [1]     [2]       [3]     [4]
 *   OUT1    OUT2    OUT3      OUT4    OUT5
 *   R2      R1      中心       L1      L2
 *   (最右)                          (最左)
 *
 * 电平逻辑: 黑线=1 (传感器触发), 白底=0
 */

#ifndef __TRACKING_H
#define __TRACKING_H

#include "init.h"        /* 引脚宏来自 init.h */
#include "encoder.h"     /* Motor_Load() */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 循迹标志结构体
 * ================================================================ */
typedef struct {
    uint8_t is_end;          /* 检测到停车线 */
    uint8_t is_r;            /* 检测到右急转弯 */
    uint8_t is_l;            /* 检测到左急转弯 */
    uint8_t little_end;      /* 小圆点检测 */
    uint8_t stop_phase;      /* 停车阶段: 0=无, 1=刹车, 2=回退, 3=完成 */
} track_flags_t;

extern volatile track_flags_t track_flags;

/* 循迹控制变量 (跨上下文共享, volatile: 主循环写, TIMG6 中断读) */
extern volatile uint8_t tracking_enabled;       /* 循迹启停: 1=运行, 0=停止 */
extern volatile int32_t target_speed;        /* 目标速度 (动态, 弯道减速) */
extern volatile int32_t track_error;         /* 循迹偏差 (供外部 PID 使用) */
extern volatile float   last_track_error;    /* 上次偏差 (丢线衰减用) */

/* ================================================================
 * API
 * ================================================================ */

void Tracking_Init(void);
void Tracking_Read(void);
void Tracking_Tick(void);
void Tracking_Reset(void);

/* 任务管理接口 (供 task_manager 调用) */
void Tracking_Enable(uint8_t en);       /* 1=启动循迹, 0=停止 */
void Tracking_SetBaseSpeed(int32_t spd);/* 设置基础速度 (0~200) */
uint8_t Tracking_IsEnabled(void);       /* 返回 tracking_enabled */
uint8_t Tracking_IsStopped(void);       /* 返回 is_end && !s_user_stopped */

#ifdef __cplusplus
}
#endif

#endif /* __TRACKING_H */
