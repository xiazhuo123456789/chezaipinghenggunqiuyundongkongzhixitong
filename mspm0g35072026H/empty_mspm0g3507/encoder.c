/*
 * encoder.c — Motor PWM init + Encoder init + Quadrature decoder + Motor control
 * MSPM0G3507 (天猛星 3507)
 *
 * PWM:
 *   PWMA: PB14 → TIMG12_CCP1, 10kHz
 *   PWMB: PA7  → TIMG8_CC0,  10kHz
 *   (PB14 only has TIMG12_CCP1; PA7 has TIMG8_CC0 at PF=4)
 *
 * Direction: PB9(AIN1) PB12(AIN2) PB7(BIN1) PB6(BIN2) ← SysConfig
 * Encoder:  PB10/PB11 (left), PB4/PB5 (right) + TIMG6 sync @ 100Hz
 */

#include "encoder.h"
#include "tracking.h"
#include "key.h"
#include <string.h>

static volatile Encoder Enc_L, Enc_R;
volatile uint32_t g_uptime_ms = 0;  /* 系统运行毫秒, 每 10ms+10 */

/* ================================================================
 * Motor_PWM_Init — 手动配两个独立 PWM 通道
 *
 * TIMG12: prescale=15, period=199 → 32M/16/200 = 10kHz
 *          CC1→PB14 (PWMA), CC0 unused
 * TIMG8:  prescale=15, period=199 → 10kHz
 *          CC0→PA7 (PWMB), CC1 unused
 * ================================================================ */
void Motor_PWM_Init(void)
{
    /* ---------- TIMG12: PWMA (PB14) ---------- */
    DL_TimerG_reset(PWM_A_INST);
    DL_TimerG_enablePower(PWM_A_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    {
        static const DL_TimerG_ClockConfig ck = {
            .clockSel = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale = 15U
        };
        static const DL_TimerG_PWMConfig pwm = {
            .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
            .period = 199,
            .isTimerWithFourCC = false,
            .startTimer = DL_TIMER_STOP
        };
        DL_TimerG_setClockConfig(PWM_A_INST, (DL_TimerG_ClockConfig*)&ck);
        DL_TimerG_initPWMMode(PWM_A_INST, (DL_TimerG_PWMConfig*)&pwm);
    }

    DL_TimerG_setCounterControl(PWM_A_INST,
        DL_TIMER_CZC_CCCTL0_ZCOND,
        DL_TIMER_CAC_CCCTL0_ACOND,
        DL_TIMER_CLC_CCCTL0_LCOND);

    /* CC0 — unused, PP output */
    DL_TimerG_setCaptureCompareOutCtl(PWM_A_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_HIGH,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptCompUpdateMethod(PWM_A_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_A_INST, 0, DL_TIMER_CC_0_INDEX);

    /* CC1 → PB14 (PWMA), duty=0 at start */
    DL_TimerG_setCaptureCompareOutCtl(PWM_A_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_HIGH,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptCompUpdateMethod(PWM_A_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_A_INST, 0, PWM_A_CC1);

    DL_TimerG_setCCPDirection(PWM_A_INST, DL_TIMER_CC1_OUTPUT);
    DL_TimerG_enableClock(PWM_A_INST);

    /* IOMUX */
    DL_GPIO_initPeripheralOutputFunction(PWM_A_IOMUX, PWM_A_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIOB, PWM_A_PIN);

    /* ---------- TIMG8: PWMB (PA7) ---------- */
    DL_TimerG_reset(PWM_B_INST);
    DL_TimerG_enablePower(PWM_B_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    {
        static const DL_TimerG_ClockConfig ck = {
            .clockSel = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale = 15U
        };
        static const DL_TimerG_PWMConfig pwm = {
            .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
            .period = 199,
            .isTimerWithFourCC = false,
            .startTimer = DL_TIMER_STOP
        };
        DL_TimerG_setClockConfig(PWM_B_INST, (DL_TimerG_ClockConfig*)&ck);
        DL_TimerG_initPWMMode(PWM_B_INST, (DL_TimerG_PWMConfig*)&pwm);
    }

    DL_TimerG_setCounterControl(PWM_B_INST,
        DL_TIMER_CZC_CCCTL0_ZCOND,
        DL_TIMER_CAC_CCCTL0_ACOND,
        DL_TIMER_CLC_CCCTL0_LCOND);

    /* CC0 → PA7 (PWMB), duty=0 at start */
    DL_TimerG_setCaptureCompareOutCtl(PWM_B_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_HIGH,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptCompUpdateMethod(PWM_B_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_B_INST, 0, DL_TIMER_CC_0_INDEX);

    /* CC1 — unused */
    DL_TimerG_setCaptureCompareOutCtl(PWM_B_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptCompUpdateMethod(PWM_B_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_B_INST, 0, DL_TIMER_CC_1_INDEX);

    DL_TimerG_setCCPDirection(PWM_B_INST, DL_TIMER_CC0_OUTPUT);
    DL_TimerG_enableClock(PWM_B_INST);

    /* IOMUX */
    DL_GPIO_initPeripheralOutputFunction(PWM_B_IOMUX, PWM_B_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIOA, PWM_B_PIN);

    /* 启动两个 PWM 计数器 */
    DL_TimerG_startCounter(PWM_A_INST);
    DL_TimerG_startCounter(PWM_B_INST);
}


/* ================================================================
 * Encoder_Init — 编码器 GPIO 输入 + 同步定时器
 *
 * 左侧: PB10/PB11 → 数字输入 + 双边沿中断
 * 右侧: PB4/PB5   → 数字输入 + 双边沿中断
 * Sync: TIMG6 @ 100Hz (10ms)
 * ================================================================ */
void Encoder_Init(void)
{
    memset((void*)&Enc_L, 0, sizeof(Enc_L));
    memset((void*)&Enc_R, 0, sizeof(Enc_R));

    /* PB10: 左 A */
    DL_GPIO_initDigitalInputFeatures(ENC_L_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableInterrupt(ENCODER_PORT, ENC_L_A_PIN);

    /* PB11: 左 B */
    DL_GPIO_initDigitalInputFeatures(ENC_L_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableInterrupt(ENCODER_PORT, ENC_L_B_PIN);

    /* PB4: 右 A */
    DL_GPIO_initDigitalInputFeatures(ENC_R_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableInterrupt(ENCODER_PORT, ENC_R_A_PIN);

    /* PB5: 右 B */
    DL_GPIO_initDigitalInputFeatures(ENC_R_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableInterrupt(ENCODER_PORT, ENC_R_B_PIN);

    /* TIMG6: 同步定时器 @100Hz */
    DL_TimerG_reset(ENC_TIMER_INST);
    DL_TimerG_enablePower(ENC_TIMER_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    {
        static const DL_TimerG_ClockConfig ck = {
            .clockSel = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale = 31U
        };
        DL_TimerG_setClockConfig(ENC_TIMER_INST, (DL_TimerG_ClockConfig*)&ck);
    }
    DL_TimerG_setLoadValue(ENC_TIMER_INST, 9999);
    DL_TimerG_enableInterrupt(ENC_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(ENC_TIMER_INST);

    /* 使能中断 */
    NVIC_ClearPendingIRQ(ENC_INT_IRQN);
    NVIC_ClearPendingIRQ(ENC_TIMER_IRQN);
    NVIC_EnableIRQ(ENC_INT_IRQN);
    NVIC_EnableIRQ(ENC_TIMER_IRQN);

    DL_TimerG_startCounter(ENC_TIMER_INST);
}


/* ================================================================
 * Motor_Get_Encoder
 * ================================================================ */
int Motor_Get_Encoder(int dir)
{
    return dir ? Enc_R.Obtained_Get_Encoder_Count
               : Enc_L.Obtained_Get_Encoder_Count;
}


/* ================================================================
 * GROUP1_IRQHandler — GPIOB 中断 (正交编码器解码)
 *
 * PB10=E1A, PB11=E1B → Enc_L (左侧)
 * PB4=E2A,  PB5=E2B  → Enc_R (右侧)
 * ================================================================ */
void GROUP1_IRQHandler(void)
{
    uint32_t f = DL_GPIO_getEnabledInterruptStatus(
        ENCODER_PORT,
        ENC_L_A_PIN | ENC_L_B_PIN | ENC_R_A_PIN | ENC_R_B_PIN);

    /* Left (PB10/PB11) */
    if (f & ENC_L_A_PIN)
        Enc_L.Should_Get_Encoder_Count +=
            DL_GPIO_readPins(ENCODER_PORT, ENC_L_B_PIN) ? 1 : -1;
    else if (f & ENC_L_B_PIN)
        Enc_L.Should_Get_Encoder_Count +=
            DL_GPIO_readPins(ENCODER_PORT, ENC_L_A_PIN) ? -1 : 1;

    /* Right (PB4/PB5) */
    if (f & ENC_R_A_PIN)
        Enc_R.Should_Get_Encoder_Count +=
            DL_GPIO_readPins(ENCODER_PORT, ENC_R_B_PIN) ? -1 : 1;
    else if (f & ENC_R_B_PIN)
        Enc_R.Should_Get_Encoder_Count +=
            DL_GPIO_readPins(ENCODER_PORT, ENC_R_A_PIN) ? 1 : -1;

    DL_GPIO_clearInterruptStatus(ENCODER_PORT, f);
}


/* ================================================================
 * Encoder_Sync — 编码器同步 (供主文件 TIMG6_IRQHandler 调用)
 *
 * 职责: 原始计数→有效计数, 清零原始计数
 * 调用方: empty_mspm0g3507.c 的 TIMG6_IRQHandler
 * ================================================================ */
void Encoder_Sync(void)
{
    Enc_L.Obtained_Get_Encoder_Count = Enc_L.Should_Get_Encoder_Count;
    Enc_R.Obtained_Get_Encoder_Count = -Enc_R.Should_Get_Encoder_Count;
    Enc_L.Should_Get_Encoder_Count = 0;
    Enc_R.Should_Get_Encoder_Count = 0;
}


/* ================================================================
 * Limit
 * ================================================================ */
void Limit(int *motoA, int *motoB)
{
    *motoA = (*motoA > PWM_MAX) ? PWM_MAX : ((*motoA < PWM_MIN) ? PWM_MIN : *motoA);
    *motoB = (*motoB > PWM_MAX) ? PWM_MAX : ((*motoB < PWM_MIN) ? PWM_MIN : *motoB);
}


/* ================================================================
 * Motor_Load — 设置双电机 PWM + 方向
 *
 * left:  TIMG12 CC1 → PB14 (PWMA), 正=前进
 * right: TIMG8  CC0 → PA7  (PWMB), 正=前进
 *
 * 并发安全: 方向引脚 + PWM 写入原子化, 防止中断内外并发调用
 *           导致方向/PWM 中间状态 (桥臂瞬间短路风险)
 * ================================================================ */
void Motor_Load(int32_t left, int32_t right)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();  /* 临界区: 方向 + PWM = 原子操作 */

    /* Left motor (A/PB14)  — TB6612: IN1=1,IN2=0=CW(前), IN1=0,IN2=1=CCW(后) */
    if (left > 0)       { AIN1_SET(1); AIN2_SET(0); }   /* 正转 */
    else if (left < 0)  { AIN1_SET(0); AIN2_SET(1); }   /* 反转 */
    else                { AIN1_SET(0); AIN2_SET(0); }   /* 刹车 */
    DL_TimerG_setCaptureCompareValue(PWM_A_INST, ABS(left), PWM_A_CC1);

    /* Right motor (B/PA7) */
    if (right > 0)      { BIN1_SET(1); BIN2_SET(0); }   /* 正转 */
    else if (right < 0) { BIN1_SET(0); BIN2_SET(1); }   /* 反转 */
    else                { BIN1_SET(0); BIN2_SET(0); }   /* 刹车 */
    DL_TimerG_setCaptureCompareValue(PWM_B_INST, ABS(right), PWM_B_CC0);

    if (!primask) __enable_irq();  /* 恢复中断 */
}