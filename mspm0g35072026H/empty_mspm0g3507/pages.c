/*
 * pages.c — 合并所有页面/菜单/显示实现
 *
 * 页面 1: Hello World
 * 页面 2: 循迹 (TASK_2)
 * 页面 3: 舵机测试
 * 页面 4: 钢珠摆杆 (TASK_3)
 * 页面 5: 视觉跟随 (TASK_4/5/6)
 *
 * 按键事件: 所有页面共用 HandleKey
 * 页面活跃: Page_Run 按页面分发
 * 显示刷新: 各页面内部自行刷新 (无需全局 DisplayUpdate)
 */
#include "pages.h"
#include "encoder.h"
#include "tracking.h"
#include "oled.h"
#include "key.h"
#include "servo.h"
#include "uart.h"
#include "task_manager.h"

/* ================================================================
 * 视觉闭环全局变量 (被 task_manager 使用)
 * ================================================================ */
volatile int16_t g_vision_target_pos = 0;
volatile int16_t g_vision_ball_vel  = 0;
volatile uint16_t g_vision_stay_cnt = 0;
volatile uint8_t  g_vision_timeout  = 0;

/* ================================================================
 * 共享状态
 * ================================================================ */
static uint8_t  s_menu_page = 1;

/* ---------- 页面 2 靠 Task_Tick ---------- */

/* ---------- 页面 3: 舵机测试 ---------- */
static uint16_t s_servo_angle = 90;
static uint16_t s_servo_last  = 255;
static uint8_t  s_servo_init  = 0;

/* ---------- 页面 5: 视觉跟随 ---------- */
static volatile uint8_t  s_vision_active = 0;
static volatile int    s_vision_last_disp_dx = 0;
static volatile int16_t s_vision_last_disp_vel = 0;
static volatile uint16_t s_vision_last_disp_stay = 0;

/* 视觉 PID 持久状态 (模块级, Vision_Enable/Vision_Reset 负责清零) */
static uint32_t s_vision_last_rx_ms  = 0;   /* 上次收到 K230 帧的时间戳 */
static float    s_vision_integral    = 0.0f; /* 积分项 */
static float    s_vision_prev_err    = 0.0f; /* 上次误差 (D 项用) */
static float    s_vision_last_pos    = 0.0f; /* 上次球位置 (速度估计用) */
static uint8_t  s_vision_was_active  = 0;    /* 上次是否活跃 (检测 0→1 跳变) */

#define VISION_PAGE         5
#define VISION_PID_KP       0.12f
#define VISION_PID_KD       0.36f
#define VISION_PID_KI       0.002f
#define VISION_SERVO_MID    90.0f
#define VISION_ANGLE_MIN    15.0f
#define VISION_ANGLE_MAX    165.0f
#define VISION_DEADBAND_DX  5
#define VISION_DEADBAND_VEL 1.0f
#define VISION_TIMEOUT_MS   5000U
#define VISION_INTEGRAL_MAX 200.0f
#define VISION_CORR_LIMIT   20.0f
#define VISION_TIMER        (TIMG7)
#define VISION_TIMER_IRQN   (TIMG7_INT_IRQn)

#define PAGE_COUNT          5

/* ================================================================
 * 表驱动菜单 (借鉴基础版本 Key_table, 扩展 on_key/on_run/on_leave)
 *
 * 加新页面只需在 s_page_table 加 1 行 + 实现 3 个函数:
 *   on_key  — 按键处理 (KEY2进, KEY3退, 页面特定逻辑)
 *   on_run  — 每帧活跃逻辑 (状态机推进, 显示刷新)
 *   on_leave — 离开页面时清理 (Task_Stop, 舵机归位等)
 * ================================================================ */
typedef void (*Page_OnKey)(uint8_t key);
typedef void (*Page_OnRun)(void);
typedef void (*Page_OnLeave)(void);

typedef struct {
    uint8_t      next_page;  /* KEY4 切到的下一页 (编号) */
    Page_OnKey   on_key;     /* 按键处理函数 */
    Page_OnRun   on_run;     /* 每帧活跃逻辑 */
    Page_OnLeave on_leave;   /* 离开页面时清理 */
} Page_Entry;

/* 前向声明 */
static void Page1_OnKey(uint8_t key);
static void Page2_OnKey(uint8_t key);
static void Page3_OnKey(uint8_t key);
static void Page4_OnKey(uint8_t key);
static void Page5_OnKey(uint8_t key);
static void Page1_Run(void);
static void Page2_Run(void);
static void Page3_Run(void);
static void Page4_Run(void);
static void Page5_Run(void);
static void Page1_OnLeave(void);
static void Page2_OnLeave(void);
static void Page3_OnLeave(void);
static void Page4_OnLeave(void);
static void Page5_OnLeave(void);

static const Page_Entry s_page_table[PAGE_COUNT] = {
    { 2, Page1_OnKey, Page1_Run, Page1_OnLeave },  /* 页面1: Hello */
    { 3, Page2_OnKey, Page2_Run, Page2_OnLeave },  /* 页面2: 循迹 */
    { 4, Page3_OnKey, Page3_Run, Page3_OnLeave },  /* 页面3: 舵机测试 */
    { 5, Page4_OnKey, Page4_Run, Page4_OnLeave },  /* 页面4: 钢球摆杆 */
    { 1, Page5_OnKey, Page5_Run, Page5_OnLeave },  /* 页面5: 视觉跟随 */
};


/* ================================================================
 * 按键: 表查找 (15 行)
 * ================================================================ */
static void HandleKey(uint8_t key)
{
    if (key == KEY_NONE) return;

    if (key == KEY4_PRESS) {
        const Page_Entry *cur = &s_page_table[s_menu_page - 1];
        cur->on_leave();                    /* 清理当前页 */
        s_menu_page = cur->next_page;       /* 查表跳转 */
        OLED_ShowPage(s_menu_page);
        return;
    }

    s_page_table[s_menu_page - 1].on_key(key);   /* 页面自己的按键处理 */
}


/* ================================================================
 * 页面 1: Hello World — 什么都不做
 * ================================================================ */
static void Page1_OnKey(uint8_t key)   { (void)key; }
static void Page1_Run(void)            {}
static void Page1_OnLeave(void)        {}

/* ================================================================
 * 页面 2: 循迹 (TASK_2)
 * ================================================================ */
static void Page2_OnKey(uint8_t key)
{
    if (key == KEY2_PRESS) {
        Task_Start(TASK_2);
        OLED_ShowString(0, 2, "T2: GO", 0); OLED_Refresh();
    } else if (key == KEY3_PRESS) {
        Task_Stop();
        OLED_ShowString(0, 2, "T2: STOP", 0); OLED_Refresh();
    }
}

static void Page2_Run(void)
{
    Task_DisplayStatus();
}

static void Page2_OnLeave(void)
{
    Task_Stop();
}

/* ================================================================
 * 页面 3: 舵机测试
 * ================================================================ */
static void Page3_OnKey(uint8_t key)
{
    if (key == KEY2_PRESS) {
        if (s_servo_angle <= 175) s_servo_angle += 5;
        Servo_Set_Angle(s_servo_angle);
    } else if (key == KEY3_PRESS) {
        if (s_servo_angle >= 5) s_servo_angle -= 5;
        Servo_Set_Angle(s_servo_angle);
    }
}

static void Page3_Run(void)
{
    if (!s_servo_init) {
        s_servo_init  = 1;
        s_servo_angle = 90;
        s_servo_last  = 255;
        Servo_Set_Angle(s_servo_angle);
        OLED_Clear();
        OLED_ShowString(0, 0, "SERVO TEST", 0);
        OLED_Refresh();
    }

    if (s_servo_angle != s_servo_last) {
        s_servo_last = s_servo_angle;
        uint32_t cc = (uint32_t)s_servo_angle * SERVO_CC_PER_DEG_X100 / 100U + SERVO_CC_MIN;
        OLED_ShowString(0, 2, "Angle:", 0);
        OLED_ShowNum(48, 2, s_servo_angle, 3, 0);
        OLED_ShowString(72, 2, "deg", 0);
        OLED_ShowString(0, 4, "CC:", 0);
        OLED_ShowNum(24, 4, (int32_t)cc, 5, 0);
        OLED_Refresh();
    }
}

static void Page3_OnLeave(void)
{
    Servo_Set_Angle(90);
    s_servo_init = 0;
}

/* ================================================================
 * 页面 4: 钢球摆杆 (TASK_3)
 * ================================================================ */
static void Page4_OnKey(uint8_t key)
{
    if (key == KEY2_PRESS) {
        Task_Start(TASK_3);
        OLED_ShowString(0, 2, "T3: GO", 0); OLED_Refresh();
    } else if (key == KEY3_PRESS) {
        Task_Stop();
        OLED_ShowString(0, 2, "T3: RESET", 0); OLED_Refresh();
    }
}

static void Page4_Run(void)
{
    Task_DisplayStatus();
}

static void Page4_OnLeave(void)
{
    Task_Stop();
}

/* ================================================================
 * 页面 5: 视觉跟随 (TASK_4/5/6)
 * ================================================================ */
static void Page5_OnKey(uint8_t key)
{
    if (key == KEY2_PRESS) {
        Task_Start(TASK_4);
        OLED_ShowString(0, 2, "T4: GO", 0); OLED_Refresh();
    } else if (key == KEY3_PRESS) {
        Task_Stop();
        OLED_ShowString(0, 2, "STOP", 0); OLED_Refresh();
    }
}

static void Page5_Run(void)
{
    Task_DisplayStatus();

    if (s_vision_active) {
        int dx = UART_Get_Dx();
        int16_t vel = g_vision_ball_vel;
        uint16_t stay = g_vision_stay_cnt;
        if (dx != s_vision_last_disp_dx || vel != s_vision_last_disp_vel || stay != s_vision_last_disp_stay) {
            s_vision_last_disp_dx = dx; s_vision_last_disp_vel = vel; s_vision_last_disp_stay = stay;
            OLED_ShowString(0, 0, "VISION", 0);
            OLED_ShowString(0, 2, "dx:",0); OLED_ShowNum(18,2,(int32_t)dx,4,0);
            OLED_ShowString(0, 4, "vel:",0); OLED_ShowNum(30,4,(int32_t)(vel/100),4,0);
            OLED_ShowString(0, 6, "st:",0); OLED_ShowNum(18,6,(int32_t)stay,4,0);
            OLED_Refresh();
        }
    }
}

static void Page5_OnLeave(void)
{
    if (s_vision_active) {
        s_vision_active = 0;
    }
    Task_Stop();
}

/* ================================================================
 * 页面 5 视觉 PID (TIMG7 50Hz)
 * ================================================================ */
void Page5_TimerInit(void)
{
    DL_TimerG_reset(VISION_TIMER);
    DL_TimerG_enablePower(VISION_TIMER);
    delay_cycles(POWER_STARTUP_DELAY);

    {
        static const DL_TimerG_ClockConfig ck = {
            .clockSel = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale = 31U
        };
        DL_TimerG_setClockConfig(VISION_TIMER, (DL_TimerG_ClockConfig*)&ck);
    }
    DL_TimerG_setLoadValue(VISION_TIMER, 19999);
    DL_TimerG_enableInterrupt(VISION_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(VISION_TIMER);

    NVIC_ClearPendingIRQ(VISION_TIMER_IRQN);
    NVIC_EnableIRQ(VISION_TIMER_IRQN);
    DL_TimerG_startCounter(VISION_TIMER);
}

/*
 * Vision_Seq_Tick 在 task_manager.c 中实现, 负责稳球序列状态机
 * Vision_Tick 在此实现, 负责 PID 计算
 *
 * TIMG7_IRQHandler 调用顺序: Vision_Seq_Tick → Vision_Tick
 * Vision_Tick 不重复调用 Vision_Seq_Tick (修复双重调用)
 */
extern void Vision_Seq_Tick(void);

void Vision_Tick(void)
{
    if (!s_vision_active) {
        s_vision_was_active = 0;    /* 任务停止 → 为下次 0→1 跳变做准备 */
        g_vision_ball_vel = 0;
        return;
    }

    /* 检测 0→1 跳变: 重新激活时重置所有持久状态 (第一帧跳过 PID, 等下一帧) */
    if (!s_vision_was_active) {
        s_vision_last_rx_ms = g_uptime_ms;
        s_vision_integral   = 0.0f;
        s_vision_prev_err   = 0.0f;
        s_vision_last_pos   = 0.0f;
        s_vision_was_active = 1;
        g_vision_ball_vel   = 0;
        return;  /* 跳变帧跳过 PID, 避免未刷新的 dx 导致首帧异常 */
    }

    int dx = UART_Get_Dx();

    /* 超时保护 */
    if (g_uptime_ms - s_vision_last_rx_ms > VISION_TIMEOUT_MS) {
        g_vision_timeout = 1;
        s_vision_active = 0;
        Task_Stop();
        Servo_Set_AngleF(VISION_SERVO_MID);
        return;
    }
    s_vision_last_rx_ms = g_uptime_ms;

    float ball_pos = (float)dx;

    /* 速度估计 (一阶低通) */
    float vel_raw = s_vision_last_pos != 0.0f ? ball_pos - s_vision_last_pos : 0.0f;
    float vel_now = 0.6f * (float)g_vision_ball_vel + 0.4f * vel_raw;
    g_vision_ball_vel = (int16_t)(vel_now * 100.0f);
    s_vision_last_pos = ball_pos;

    float error = ball_pos - (float)g_vision_target_pos;

    /* 死区 */
    if (fabsf(error) <= (float)VISION_DEADBAND_DX
        && fabsf(vel_now) < VISION_DEADBAND_VEL) {
        g_vision_stay_cnt++;
        Servo_Set_AngleF(VISION_SERVO_MID);
        return;
    }
    g_vision_stay_cnt = 0;

    /* PID */
    float p_out = VISION_PID_KP * error;
    float d_out = VISION_PID_KD * (error - s_vision_prev_err);
    s_vision_prev_err = error;

    if (fabsf(error) < 30.0f) s_vision_integral += error * 0.3f;
    else s_vision_integral *= 0.5f;
    if (s_vision_integral >  VISION_INTEGRAL_MAX) s_vision_integral =  VISION_INTEGRAL_MAX;
    if (s_vision_integral < -VISION_INTEGRAL_MAX) s_vision_integral = -VISION_INTEGRAL_MAX;
    float i_out = VISION_PID_KI * s_vision_integral;

    float correction = p_out + i_out + d_out;
    if (correction >  VISION_CORR_LIMIT) correction =  VISION_CORR_LIMIT;
    if (correction < -VISION_CORR_LIMIT) correction = -VISION_CORR_LIMIT;

    float angle = VISION_SERVO_MID - correction;
    if (angle < VISION_ANGLE_MIN) angle = VISION_ANGLE_MIN;
    if (angle > VISION_ANGLE_MAX) angle = VISION_ANGLE_MAX;

    Servo_Set_AngleF(angle);
}

/* ================================================================
 * 对外 API
 * ================================================================ */
void Pages_HandleKey(uint8_t key)    { HandleKey(key); }
void Pages_RunActivePage(void)
{
    s_page_table[s_menu_page - 1].on_run();   /* 1 行表查找: 当前页面的活跃逻辑 */
}
