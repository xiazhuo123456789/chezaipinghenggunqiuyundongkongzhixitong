/*
 * servo.c — 舵机 PWM 初始化 + 角度控制
 * MSPM0G3507
 *
 * 定时器: TIMA0, 50Hz (20ms 周期), 向上计数边沿对齐
 * 引脚:   PA8 → TIMA0_CCP0 (IOMUX_PINCM19, PF=5)
 *
 * PWM 模式: EDGE_ALIGN_UP (向上计数)
 *   counter: 0 → LOAD(63998), Zero 时 CCP=HIGH, CC match 时 CCP=LOW
 *   CC 值 == HIGH 持续 tick 数 (即舵机脉冲宽度)
 *
 * 高精度模式:
 *   时钟: 32MHz / 10 = 3.2MHz (312.5ns/tick)
 *   周期: 63999 — 刚好填满 16 位 CCR
 *   分辨率: ~0.028°/tick (每度约 35.56 tick)
 *
 * 脉宽映射:
 *   0° → 0.5ms (CC=1600)
 *   90° → 1.5ms (CC=4800)
 *   180° → 2.5ms (CC=8000)
 *   CC = 1600 + angle * 6400 / 180
 */

#include "servo.h"

/* ================================================================
 * Servo_PWM_Init — 初始化 TIMA0 为 50Hz 高精度 PWM
 *
 * 时钟: 32MHz / 10 / 64000 = 50Hz
 * CC0 → PA8, 初始占空比 = 1.5ms (90°)
 * ================================================================ */
void Servo_PWM_Init(void)
{
    /* 复位并使能 TIMA0 */
    DL_TimerA_reset(SERVO_PWM_INST);
    DL_TimerA_enablePower(SERVO_PWM_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    /* 时钟: prescale=9 (÷10), period=63999 → 50Hz */
    {
        static const DL_TimerA_ClockConfig ck = {
            .clockSel    = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale    = SERVO_PRESCALE
        };
        static const DL_TimerA_PWMConfig pwm = {
            .pwmMode           = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
            .period            = SERVO_PERIOD,
            .isTimerWithFourCC = false,
            .startTimer        = DL_TIMER_STOP
        };
        DL_TimerA_setClockConfig(SERVO_PWM_INST, (DL_TimerA_ClockConfig*)&ck);
        DL_TimerA_initPWMMode(SERVO_PWM_INST, (DL_TimerA_PWMConfig*)&pwm);
    }

    /* 计数器控制: 零→高, 匹配→低 (标准 PWM) */
    DL_TimerA_setCounterControl(SERVO_PWM_INST,
        DL_TIMER_CZC_CCCTL0_ZCOND,
        DL_TIMER_CAC_CCCTL0_ACOND,
        DL_TIMER_CLC_CCCTL0_LCOND);

    /* CC0 → PA8: 初始高电平(短脉冲), 匹配后拉低 */
    DL_TimerA_setCaptureCompareOutCtl(SERVO_PWM_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_HIGH,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        SERVO_PWM_CC);
    DL_TimerA_setCaptCompUpdateMethod(SERVO_PWM_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, SERVO_PWM_CC);
    DL_TimerA_setCaptureCompareValue(SERVO_PWM_INST, SERVO_CC_MID, SERVO_PWM_CC);

    /* 方向: 输出 */
    DL_TimerA_setCCPDirection(SERVO_PWM_INST, DL_TIMER_CC0_OUTPUT);
    DL_TimerA_enableClock(SERVO_PWM_INST);

    /* IOMUX: PA8 → TIMA0_CCP0 (PF=5) */
    DL_GPIO_initPeripheralOutputFunction(SERVO_IOMUX, SERVO_IOMUX_FUNC);
    DL_GPIO_enableOutput(SERVO_PORT, SERVO_PIN);

    /* 启动 PWM */
    DL_TimerA_startCounter(SERVO_PWM_INST);
}

/* ================================================================
 * Servo_Set_Angle — 设角度 (0°~180°, 精度 ~0.028°)
 *
 * 用定点乘法避免浮点: CC = 1600 + angle * 6400 / 180
 * 6400/180 ≈ 35.555... → 用 35556/1000 近似, 误差 < 0.003°
 * ================================================================ */
void Servo_Set_Angle(uint16_t angle)
{
    uint32_t cc;

    if (angle > 180) angle = 180;

    /* CC = 1600 + angle * 3556 / 100 (SERVO_CC_PER_DEG_X100=3556) */
    cc = (uint32_t)angle * SERVO_CC_PER_DEG_X100 / 100U + SERVO_CC_MIN;
    if (cc > SERVO_CC_MAX) cc = SERVO_CC_MAX;

    DL_TimerA_setCaptureCompareValue(SERVO_PWM_INST, cc, SERVO_PWM_CC);
}

/* ================================================================
 * Servo_Set_AngleF — 浮点角度接口 (支持小数度, 如 81.5°)
 *
 * 用于需要亚度级精度的场景 (如开环摆杆最终稳定角)。
 * 浮点仅在调用时使用一次, 不进入热点循环, 开销可忽略。
 * ================================================================ */
void Servo_Set_AngleF(float angle)
{
    float cc;

    if (angle < 0.0f)    angle = 0.0f;
    if (angle > 180.0f)  angle = 180.0f;

    /* CC = 1600 + angle * 35.56 */
    cc = (float)SERVO_CC_MIN + angle * ((float)SERVO_CC_PER_DEG_X100 / 100.0f);
    if (cc > (float)SERVO_CC_MAX) cc = (float)SERVO_CC_MAX;

    DL_TimerA_setCaptureCompareValue(SERVO_PWM_INST, (uint32_t)cc, SERVO_PWM_CC);
}

/* ================================================================
 * Servo_Set_Pulse — 原始 CC 值写入 (1600~8000)
 * ================================================================ */
void Servo_Set_Pulse(uint16_t cc_val)
{
    if (cc_val < SERVO_CC_MIN) cc_val = SERVO_CC_MIN;
    if (cc_val > SERVO_CC_MAX) cc_val = SERVO_CC_MAX;
    DL_TimerA_setCaptureCompareValue(SERVO_PWM_INST, cc_val, SERVO_PWM_CC);
}
