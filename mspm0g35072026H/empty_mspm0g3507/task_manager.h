/*
 * task_manager.h — 6 任务统一调度器
 *
 * 架构: Task_Start(id) → 内部配置循迹+稳球 → Task_Tick (TIMG6 调用)
 *
 * 任务 1: 无线图传 (不用管)
 * 任务 2: 只寻迹
 * 任务 3: 钢珠 0→+5→-5 (稳球, 不循迹)
 * 任务 4: 寻迹+稳球 (低速)
 * 任务 5: 寻迹+稳球 (中速)
 * 任务 6: 寻迹+稳球 (高速)
 */

#ifndef __TASK_MANAGER_H
#define __TASK_MANAGER_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 任务 ID 枚举
 * ================================================================ */
typedef enum {
    TASK_NONE = 0,
    TASK_2    = 2,    /* 只寻迹 */
    TASK_3    = 3,    /* 钢珠 0→+5→-5 (稳球) */
    TASK_4    = 4,    /* 寻迹+稳球 (低速) */
    TASK_5    = 5,    /* 寻迹+稳球 (中速) */
    TASK_6    = 6,    /* 寻迹+稳球 (高速) */
} TaskID;

/* ================================================================
 * API
 * ================================================================ */
void Task_Start(TaskID id);        /* 启动任务 (自动配置循迹+稳球) */
void Task_Stop(void);              /* 停止当前任务 */
void Task_Tick(void);              /* 每 10ms 调用 (在 TIMG6 中断) */
void Task_DisplayStatus(void);     /* OLED 显示任务状态 (主循环) */

#ifdef __cplusplus
}
#endif

#endif /* __TASK_MANAGER_H */
