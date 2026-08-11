/*
 * tracking.c — 5路红外寻迹模块
 * MSPM0G3507 (天猛星 3507)
 *
 * 架构:
 *   - Track_Sensors()  只做传感器读取 + 偏差计算 + 标志检测
 *   - 电机输出     由主循环或中断负责
 *   - tracking_enabled 控制启停, 上电默认关
 *
 * 传感器排列 (俯视, 车头朝前):
 *   Index: [0]R2  [1]R1  [2]中心  [3]L1  [4]L2
 *   Weight:  -4    -2     0      +2    +4
 *   [0] 最右 ← → [4] 最左
 */

#include "tracking.h"
#include <string.h>

volatile track_flags_t track_flags;
uint8_t mark_track[TK_SENSOR_COUNT];

/* 循迹全局控制变量 (volatile: 主循环写, TIMG6 中断读) */
volatile uint8_t tracking_enabled = 0;      /* 1=循迹运行, 0=停止 */
volatile int32_t target_speed  = 0;      /* 目标速度 (动态调整) */
volatile int32_t track_error   = 0;      /* 当前循迹偏差 (正=线偏右, 负=线偏左) */
volatile float   last_track_error = 0.0f; /* 历史偏差 (丢线衰减用) */

/* Track_Sensors 可配置基础速度 (外部 task_manager 设置) */
static int32_t s_trace_base_speed = 18;  /* 默认 18 */

static uint8_t  s_user_stopped = 0;      /* 用户手动停车标志 (区别于自然停车线检测) */

/* ---------- 权重表 (参考天猛星: -4,-4,-4,-2,+2,+4,+4,+4) ---------- */
static const int8_t sensor_weights[TK_SENSOR_COUNT] = {
    -5,   /* R2  [0] — 最右 */
    -3,   /* R1  [1] */
     0,   /* 中心 [2] — 在黑线上 */
    +3,   /* L1  [3] */
    +5,   /* L2  [4] — 最左 */
};

/* ---------- 硬件初始化 (你的原始代码不动) ---------- */
void Tracking_Init(void)
{
    memset(mark_track, 0, sizeof(mark_track));
    memset((void *)&track_flags, 0, sizeof(track_flags));
    tracking_enabled = 0;
    last_track_error    = 0.0f;
    track_error   = 0;

    DL_GPIO_initDigitalInputFeatures(TK_OUT1_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(TK_OUT2_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(TK_OUT3_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(TK_OUT4_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(TK_OUT5_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

/* ---------- 传感器读取 (逐位扫描 5 路, 存入 mark_track) ---------- */
void Tracking_Read(void)
{
    memset(mark_track, 0, sizeof(mark_track));
    if (DL_GPIO_readPins(TK_OUT1_PORT, TK_OUT1_PIN) != TK_WHITE) mark_track[0] = TK_BLACK;
    if (DL_GPIO_readPins(TK_OUT2_PORT, TK_OUT2_PIN) != TK_WHITE) mark_track[1] = TK_BLACK;
    if (DL_GPIO_readPins(TK_OUT3_PORT, TK_OUT3_PIN) != TK_WHITE) mark_track[2] = TK_BLACK;
    if (DL_GPIO_readPins(TK_OUT4_PORT, TK_OUT4_PIN) != TK_WHITE) mark_track[3] = TK_BLACK;
    if (DL_GPIO_readPins(TK_OUT5_PORT, TK_OUT5_PIN) != TK_WHITE) mark_track[4] = TK_BLACK;
}

/* ---------- 电机控制 ---------- */
void Set_Car_Speed(int32_t left, int32_t right)
{
    if (left  > PWM_MAX)  left  = PWM_MAX;
    if (left  < PWM_MIN)  left  = PWM_MIN;
    if (right > PWM_MAX)  right = PWM_MAX;
    if (right < PWM_MIN)  right = PWM_MIN;
    Motor_Load(left, right);
}

void Tracking_Reset(void)
{
    track_flags.is_end = 0;
    track_flags.is_r   = 0;
    track_flags.is_l   = 0;
    track_flags.little_end = 0;
    track_flags.stop_phase = 0;
    s_user_stopped = 0;
}

/* ================================================================
 * Track_Sensors — 循迹偏差计算 (天猛星跟踪逻辑)
 *
 * 只算偏差 + 更新标志, 不直接设 PWM。
 * tracking_enabled==0 时跳过, 电机完全不动。
 * 仅供 Tracking_Tick 内部调用 (static), 外部不可见。
 *
 * 返回之后, 由 Tracking_Tick 用 track_error 做 PD 输出。
 * ================================================================ */
static void Track_Sensors(void)
{
    /* 没有循迹启动标志 → 什么都不做 */
    if (!tracking_enabled) return;

    Tracking_Read();

    /* ---------- 1. 计算加权偏差 ---------- */
    float total_error = 0.0f;
    uint8_t active_sensors = 0;

    for (int i = 0; i < TK_SENSOR_COUNT; i++) {
        if (mark_track[i] == TK_BLACK) {
            total_error += (float)sensor_weights[i];
            active_sensors++;
        }
    }

    /* 丢线保护: 使用历史偏差指数衰减 (天猛星做法) */
    if (active_sensors == 0) {
        total_error = last_track_error * 0.9f;
    }
    last_track_error = total_error;

    /* 写入全局偏差 (给主循环的 PID 用) */
    track_error = (int32_t)total_error;

    /* ---------- 2. 动态目标速度 ---------- */
    {
        int32_t base_speed = s_trace_base_speed;
        int32_t gain       = 5;           /* 误差→减速增益 */
        target_speed = base_speed - (gain * ABS(track_error)) / 10;
        if (target_speed < 10) target_speed = 10;
        if (target_speed > base_speed + 6) target_speed = base_speed + 6;
    }

    /* ---------- 3. 停车检测 ---------- */
    {
        /* 精准停车: 除最左传感器外，剩余4个 (索引0,1,2,3) 同时碰到黑线 → 刹车+回退 */
        if (mark_track[0] == TK_BLACK && mark_track[1] == TK_BLACK
            && mark_track[2] == TK_BLACK && mark_track[3] == TK_BLACK
            && track_flags.stop_phase == 0) {
            track_flags.stop_phase = 1;
        }
    }

    /* ---------- 4. 直角弯检测 ---------- */
    if (mark_track[0] && mark_track[1])
        track_flags.is_r = 1;    /* 右侧全黑 → 线右拐 */
    if (mark_track[3] && mark_track[4])
        track_flags.is_l = 1;    /* 左侧全黑 → 线左拐 */
}

/* ================================================================
 * Tracking_Tick — 10ms 周期循迹控制 (供 TIMG6 中断调用)
 *
 * 作用:
 *   把原 main 主循环里的「Track_Sensors + 电机控制 + 停车序列」
 *   全部移到 10ms 中断里, 摆脱 OLED 软件 I2C 的 345ms 拖累。
 *
 * 调用位置: encoder.c 的 TIMG6_IRQHandler()
 * 执行频率: 100Hz (10ms 一次)
 *
 * 并发说明:
 *   - tracking_enabled / track_flags 由主循环按键设置, 中断读
 *   - 10ms 中断与主循环访问同一标志, 用 volatile 保证可见性
 *   - 主循环按键处理期间中断仍可运行, 不影响循迹连续性
 * ================================================================ */
static uint32_t s_stop_seq_start = 0;   /* 停车序列起始时间戳 */

void Tracking_Tick(void)
{
    /* 循迹未启动 → 什么都不做 (主循环按键负责启动) */
    if (!tracking_enabled) return;

    /* ---------- 1. 偏差计算 ---------- */
    Track_Sensors();

    /* ---------- 2. 精准停车序列 (三阶段) ---------- */
    if (track_flags.stop_phase > 0) {
        if (track_flags.stop_phase == 1) {
            /* 阶段1: 立即刹车 */
            Set_Car_Speed(0, 0);
            s_stop_seq_start = g_uptime_ms;
            track_flags.stop_phase = 2;
        } else if (track_flags.stop_phase == 2) {
            /* 阶段2: 回退 (抵消惯性前冲 + 万向轮右偏) */
            Set_Car_Speed(-REVERSE_SPEED, -REVERSE_SPEED);
            if (g_uptime_ms - s_stop_seq_start >= REVERSE_DURATION_MS) {
                track_flags.stop_phase = 3;
            }
        } else if (track_flags.stop_phase == 3) {
            /* 阶段3: 最终停车 */
            tracking_enabled  = 0;
            track_flags.is_end = 1;
            track_flags.stop_phase = 0;
            target_speed   = 0;
            track_error    = 0;
            last_track_error     = 0.0f;
            Set_Car_Speed(0, 0);
        }
        return;   /* 停车序列期间不走正常循迹 */
    }

    /* ---------- 3. 停车条件检测 ---------- */
    if (track_flags.is_end) {
        Set_Car_Speed(0, 0);
        return;
    }

    /* ---------- 4. 正常循迹: PD 电机控制 ---------- */
    {
        int32_t corr;
        static float s_prev_track_err = 0.0f;
        float err_f = (float)track_error;
        float d_term = TRACK_KD * (err_f - s_prev_track_err);
        s_prev_track_err = err_f;

        if (ABS(track_error) <= 3) {
            corr = 0;   /* 死区: 1~2 个中心传感器在线, 直走 */
        } else {
            corr = (int32_t)(track_error * TRACK_KP + d_term);
        }
        int32_t base = s_trace_base_speed;
        int32_t L = base - corr + TRACK_BIAS;
        int32_t R = base + corr;
        if (L > 200) L = 200; if (L < -200) L = -200;
        if (R > 200) R = 200; if (R < -200) R = -200;
        Set_Car_Speed(L, R);
    }
}

/* ================================================================
 * 任务管理接口 — 供 task_manager 调用
 * ================================================================ */
void Tracking_Enable(uint8_t en)
{
    if (en) {
        Tracking_Reset();
        s_user_stopped  = 0;
        tracking_enabled = 1;
    } else {
        tracking_enabled = 0;
        s_user_stopped  = 1;    /* 用户中止, 不混淆 is_end 语义 */
        target_speed  = 0;
        track_error   = 0;
        last_track_error    = 0.0f;
        Set_Car_Speed(0, 0);
    }
}

void Tracking_SetBaseSpeed(int32_t spd)
{
    if (spd < 10)  spd = 10;
    if (spd > 200) spd = 200;
    s_trace_base_speed = spd;
}

uint8_t Tracking_IsEnabled(void)
{
    return tracking_enabled;
}

uint8_t Tracking_IsStopped(void)
{
    /* 自然停车线触发 (非用户手动中止) */
    return track_flags.is_end && !s_user_stopped;
}
