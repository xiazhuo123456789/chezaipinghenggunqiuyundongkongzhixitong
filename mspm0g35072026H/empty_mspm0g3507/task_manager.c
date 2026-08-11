/*
 * task_manager.c — 6 任务统一调度器
 *
 * 设计原则:
 *   - Task_Start(id) 一键启动, 内部自动配置循迹速度+稳球序列
 *   - Task_Tick() 在 TIMG6 中断 (10ms) 调用, 协调循迹停车→稳球切换
 *   - 稳球状态机 (Vision 模块) 在 TIMG7 中断 (50Hz) 独立运行
 *
 * 配置表:
 *   任务 2: track=1, vision=0, speed=18
 *   任务 3: track=0, vision=1, sequence=[+80, -80]
 *   任务 4: track=1, vision=1, speed=18, sequence=[0]
 *   任务 5: track=1, vision=1, speed=25, sequence=[0]
 *   任务 6: track=1, vision=1, speed=35, sequence=[0]
 */

#include "task_manager.h"
#include "tracking.h"
#include "oled.h"
#include "servo.h"
#include <string.h>

/* ================================================================
 * 外部引用
 * ================================================================ */
extern volatile int16_t g_vision_target_pos;
extern volatile uint16_t g_vision_stay_cnt;
extern volatile uint8_t  g_vision_timeout;
extern volatile uint32_t g_uptime_ms;
extern void Servo_Set_AngleF(float angle);

/* Vision 模块控制 (定义在 pages.c) */
extern void Vision_Enable(uint8_t en);
extern void Vision_Reset(void);
extern void Vision_SetSequence(const int16_t *steps, uint8_t count);
extern uint8_t Vision_IsSequenceDone(void);

/* ================================================================
 * 稳球序列 (任务3 专用)
 * ================================================================ */
static const int16_t s_seq_task3[] = { 80, -80 };   /* 0 → +80 → -80 */

/* ================================================================
 * 任务运行时状态
 * ================================================================ */
static TaskID   s_active_task   = TASK_NONE;
static uint8_t  s_task_has_track = 0;     /* 此任务是否包含循迹 */
static uint32_t s_task_timer     = 0;     /* 任务计时 (ms), 10ms 递增 */

/* ================================================================
 * 任务配置表
 * ================================================================ */
typedef struct {
    uint8_t  has_track;      /* 是否启动循迹 */
    uint8_t  has_vision;     /* 是否启动稳球 */
    int32_t  track_speed;    /* 循迹速度 (0=不用) */
    const int16_t *vision_seq;  /* 稳球序列 (NULL=保持目标0) */
    uint8_t  vision_seq_cnt;   /* 序列步数 */
} TaskConfig;

static const TaskConfig s_task_cfg[] = {
    [TASK_2] = { 1, 0, 18, NULL,         0 },
    [TASK_3] = { 0, 1, 0,  s_seq_task3,  2 },
    [TASK_4] = { 1, 1, 18, NULL,         0 },
    [TASK_5] = { 1, 1, 25, NULL,         0 },
    [TASK_6] = { 1, 1, 35, NULL,         0 },
};

/* ================================================================
 * 稳球序列状态机 (任务3 内部)
 *
 * Keil Q3_State 移植: IDLE → MOVE_0 → HOLD_0 → MOVE_1 → HOLD_1 → DONE
 * 每个 MOVE 阶段渐进目标, 判稳后转移到 HOLD, HOLD 超时后转移下一 MOVE
 * ================================================================ */
typedef enum {
    VS_IDLE = 0,       /* 等待激活 */
    VS_MOVE,           /* 渐进到当前目标 */
    VS_HOLD,           /* 保持在当前目标 (计时) */
    VS_DONE            /* 序列完成 */
} VisionSeqState;

static const int16_t *s_vs_steps      = NULL;
static uint8_t  s_vs_step_count       = 0;
static uint8_t  s_vs_cur_idx          = 0;
static VisionSeqState s_vs_state      = VS_IDLE;
static uint32_t s_vs_hold_start       = 0;
static uint8_t  s_vs_active           = 0;

#define VS_HOLD_TIME        1000U   /* HOLD 保持时间 (ms) */
#define VS_GRADUAL_STEP     10      /* 目标渐进步长 (像素/帧, 50Hz) */
#define VS_STAY_LIMIT       30      /* 判稳阈值 (50Hz tick = 600ms) */
#define VISION_SERVO_MID    90.0f   /* 舵机机械中位 */

static void Vision_Seq_Reset(void)
{
    s_vs_state    = VS_IDLE;
    s_vs_cur_idx  = 0;
    s_vs_active   = 0;
    g_vision_target_pos = 0;
    g_vision_stay_cnt   = 0;
}

static uint8_t Vision_Seq_IsDone(void)
{
    return (s_vs_state == VS_DONE);
}

/*
 * Vision_Seq_Tick — 稳球序列状态机 (50Hz, TIMG7 中断调用)
 *
 * 在 Vision_Tick 之前调用, 负责更新 g_vision_target_pos.
 * TIMG7_IRQHandler 中调用顺序: Vision_Seq_Tick → Vision_Tick
 */
void Vision_Seq_Tick(void)
{
    if (!s_vs_active) return;
    if (!s_vs_steps || s_vs_step_count == 0) return;

    switch (s_vs_state) {
    case VS_IDLE:
        /* 首次激活: 启动第一个 MOVE */
        s_vs_cur_idx = 0;
        s_vs_state   = VS_MOVE;
        g_vision_stay_cnt = 0;
        /* fall through */

    case VS_MOVE:
        /* 渐进目标: 每次 ±10 像素 */
        {
            int16_t target = s_vs_steps[s_vs_cur_idx];
            int16_t diff   = target - g_vision_target_pos;
            if      (diff >  VS_GRADUAL_STEP) g_vision_target_pos += VS_GRADUAL_STEP;
            else if (diff < -VS_GRADUAL_STEP) g_vision_target_pos -= VS_GRADUAL_STEP;
            else                              g_vision_target_pos  = target;

            /* 判稳转移 */
            if (g_vision_target_pos == target
                && g_vision_stay_cnt >= VS_STAY_LIMIT) {
                s_vs_state        = VS_HOLD;
                s_vs_hold_start   = g_uptime_ms;
                g_vision_stay_cnt = 0;
            }
        }
        break;

    case VS_HOLD:
        /* 保持一段时间后进入下一步或完成 */
        if (g_uptime_ms - s_vs_hold_start >= VS_HOLD_TIME) {
            s_vs_cur_idx++;
            if (s_vs_cur_idx < s_vs_step_count) {
                s_vs_state        = VS_MOVE;
                g_vision_stay_cnt = 0;
            } else {
                s_vs_state        = VS_DONE;
                g_vision_target_pos = 0;  /* 回到中心 */
            }
        }
        break;

    case VS_DONE:
        /* 序列完成, 保持中心 */
        break;
    }
}

/* ================================================================
 * 对 Vision 模块暴露的接口
 * ================================================================ */
void Vision_Enable(uint8_t en)
{
    if (en) {
        Vision_Seq_Reset();
        s_vs_active          = 1;
        g_vision_timeout     = 0;
    } else {
        s_vs_active          = 0;
        g_vision_target_pos  = 0;
        Servo_Set_AngleF(VISION_SERVO_MID);
    }
}

void Vision_Reset(void)
{
    Vision_Seq_Reset();
}

void Vision_SetSequence(const int16_t *steps, uint8_t count)
{
    s_vs_steps      = steps;
    s_vs_step_count = count;
    Vision_Seq_Reset();
}

uint8_t Vision_IsSequenceDone(void)
{
    return Vision_Seq_IsDone();
}

/* ================================================================
 * Task_Start — 一键启动任务
 *
 * 根据配置表自动设置:
 *   - 循迹: Tracking_Enable + Tracking_SetBaseSpeed
 *   - 稳球: Vision_Enable + Vision_SetSequence
 *   - 计时: 清零
 * ================================================================ */
void Task_Start(TaskID id)
{
    if (id < TASK_2 || id > TASK_6) return;

    /* 先停止当前任务, 清理干净 */
    Task_Stop();

    const TaskConfig *cfg = &s_task_cfg[id];
    s_active_task    = id;
    s_task_has_track = cfg->has_track;
    s_task_timer     = 0;

    /* 循迹配置 */
    if (cfg->has_track) {
        Tracking_Reset();
        Tracking_SetBaseSpeed(cfg->track_speed);
        Tracking_Enable(1);
    }

    /* 稳球配置 */
    if (cfg->has_vision) {
        Vision_Reset();
        if (cfg->vision_seq) {
            Vision_SetSequence(cfg->vision_seq, cfg->vision_seq_cnt);
        }
        Vision_Enable(1);
    }
}

/* ================================================================
 * Task_Stop — 停止当前任务
 * ================================================================ */
void Task_Stop(void)
{
    Tracking_Enable(0);
    Vision_Enable(0);
    s_active_task    = TASK_NONE;
    s_task_has_track = 0;
    s_task_timer     = 0;
}

/* ================================================================
 * Task_Tick — 每 10ms 调用 (TIMG6 中断)
 *
 * 职责:
 *   - 任务计时
 *   - 循迹停车检测 → 自动切换稳球模式
 * ================================================================ */
void Task_Tick(void)
{
    if (s_active_task == TASK_NONE) return;

    s_task_timer += 10;

    /* 任务2: 纯循迹, 停车后自动停任务 */
    if (s_active_task == TASK_2) {
        if (Tracking_IsStopped()) {
            Task_Stop();
        }
    }
}

/* ================================================================
 * Task_DisplayStatus — OLED 显示任务状态 (主循环)
 * ================================================================ */
void Task_DisplayStatus(void)
{
    static uint32_t s_last_disp = 0xFFFFFFFF;

    if (s_active_task == TASK_NONE) {
        if (s_last_disp != 0) {
            s_last_disp = 0;
            OLED_ShowString(0, 2, "TASK: NONE", 0);
            OLED_Refresh();
        }
        return;
    }

    uint32_t s = s_task_timer / 1000;
    if (s != s_last_disp) {
        s_last_disp = s;
        OLED_ShowString(0, 2, "T:", 0);
        OLED_ShowNum(12, 2, s_active_task, 1, 0);
        OLED_ShowString(0, 4, "Time:", 0);
        OLED_ShowNum(30, 4, (int32_t)s, 4, 0);
        OLED_ShowString(56, 4, "s", 0);
        if (s_task_has_track) {
            OLED_ShowString(0, 6, "TRACK ON", 0);
        } else {
            OLED_ShowString(0, 6, "VISION ONLY", 0);
        }
        OLED_Refresh();
    }
}
