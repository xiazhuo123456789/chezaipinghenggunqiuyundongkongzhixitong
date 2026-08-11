/*
 * init.h — 全板引脚配置中心 + 统一初始化入口
 *
 * 设计原则 (江协科技风格):
 *   - 所有外设引脚宏集中定义于此 (单一真相源)
 *   - 各模块 .h 改为 #include "init.h" 获取引脚宏
 *   - main() 只调用 Board_Init() 一行完成全部硬件初始化
 *
 * 引脚总览 (MSPM0G3507):
 *   电机PWM:  PB14(TIMG12_CCP1), PA7(TIMG8_CCP0)
 *   电机方向: PB9(AIN1) PB12(AIN2) PB7(BIN1) PB6(BIN2)
 *   编码器:   PB10/PB11(左) PB4/PB5(右) + TIMG6(10ms同步)
 *   按键:     PA23(K1) PA21(K2) PB18(K3) PA17(K4)
 *   循迹:     PB19 PA16 PA14 PB17 PB20
 *   舵机:     PA8(TIMA0_CCP0)
 *   UART:     PA28(TX) PA31(RX) ← K230通信
 *   OLED:     PA0(SDA) PA1(SCL) ← 软件I2C
 */

#ifndef __INIT_H
#define __INIT_H

#include "ti_msp_dl_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 系统时钟
 * ================================================================ */
#define CPU_FREQ_HZ         32000000UL

/* ================================================================
 * 电机 PWM (TIMG12 + TIMG8, 10kHz)
 *
 * prescale=15(÷16), period=199 → 32M/16/200 = 10kHz
 * ================================================================ */
#define PWM_A_INST          (TIMG12)
#define PWM_A_CC0           DL_TIMER_CC_0_INDEX   /* CC0 unused */
#define PWM_A_CC1           DL_TIMER_CC_1_INDEX   /* CC1 → PB14 (PWMA) */
#define PWM_A_PIN           (DL_GPIO_PIN_14)
#define PWM_A_IOMUX         (IOMUX_PINCM31)
#define PWM_A_IOMUX_FUNC    IOMUX_PINCM31_PF_TIMG12_CCP1

#define PWM_B_INST          (TIMG8)
#define PWM_B_CC0           DL_TIMER_CC_0_INDEX   /* CC0 → PA7 (PWMB) */
#define PWM_B_CC1           DL_TIMER_CC_1_INDEX   /* CC1 unused */
#define PWM_B_PIN           (DL_GPIO_PIN_7)
#define PWM_B_IOMUX         (IOMUX_PINCM14)
#define PWM_B_IOMUX_FUNC    IOMUX_PINCM14_PF_TIMG8_CCP0

#define PWM_PERIOD          199
#define PWM_DUTY_MAX        200
#define PWM_MAX             200
#define PWM_MIN             (-200)
#define TRACK_DEFAULT_SPEED 120  /* 循迹默认基础速度 (供参考, 实际由 task_manager 配置) */

/* ================================================================
 * 电机方向引脚 (SysConfig 配置, 宏来自 ti_msp_dl_config.h)
 *
 * PB9=AIN1, PB12=AIN2, PB7=BIN1, PB6=BIN2
 * ================================================================ */
#define AIN1_SET(v)  ((v) ? DL_GPIO_setPins(  MOTOR_DIR_PORT, MOTOR_DIR_AIN1_PIN) \
                          : DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_DIR_AIN1_PIN))
#define AIN2_SET(v)  ((v) ? DL_GPIO_setPins(  MOTOR_DIR_PORT, MOTOR_DIR_AIN2_PIN) \
                          : DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_DIR_AIN2_PIN))
#define BIN1_SET(v)  ((v) ? DL_GPIO_setPins(  MOTOR_DIR_PORT, MOTOR_DIR_BIN1_PIN) \
                          : DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_DIR_BIN1_PIN))
#define BIN2_SET(v)  ((v) ? DL_GPIO_setPins(  MOTOR_DIR_PORT, MOTOR_DIR_BIN2_PIN) \
                          : DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_DIR_BIN2_PIN))

/* ================================================================
 * 编码器 + TIMG6 同步定时器 (10ms, 100Hz)
 *
 * 左: PB10(E1A) PB11(E1B), 右: PB4(E2A) PB5(E2B)
 * Clock: 32MHz/1/32 = 1MHz, period=9999 → 100Hz
 * ================================================================ */
#define ENCODER_PORT        (GPIOB)

#define ENC_L_A_PIN         (DL_GPIO_PIN_10)
#define ENC_L_A_IOMUX       (IOMUX_PINCM27)
#define ENC_L_B_PIN         (DL_GPIO_PIN_11)
#define ENC_L_B_IOMUX       (IOMUX_PINCM28)

#define ENC_R_A_PIN         (DL_GPIO_PIN_4)
#define ENC_R_A_IOMUX       (IOMUX_PINCM17)
#define ENC_R_B_PIN         (DL_GPIO_PIN_5)
#define ENC_R_B_IOMUX       (IOMUX_PINCM18)

#define ENC_INT_IRQN        (GPIOB_INT_IRQn)      /* GROUP1 */
#define ENC_TIMER_INST      (TIMG6)
#define ENC_TIMER_IRQN      (TIMG6_INT_IRQn)

/* ================================================================
 * 按键 (4个, 内部上拉, 按下=0)
 *
 * K1=PA23, K2=PA21, K3=PB18, K4=PA17
 * ================================================================ */
#define KEY_PORT_A          (GPIOA)
#define KEY_PORT_B          (GPIOB)

#define KEY1_PIN            (DL_GPIO_PIN_23)
#define KEY1_IOMUX          (IOMUX_PINCM50)
#define KEY1_PORT           (GPIOA)

#define KEY2_PIN            (DL_GPIO_PIN_21)
#define KEY2_IOMUX          (IOMUX_PINCM46)
#define KEY2_PORT           (GPIOA)

#define KEY3_PIN            (DL_GPIO_PIN_18)
#define KEY3_IOMUX          (IOMUX_PINCM44)
#define KEY3_PORT           (GPIOB)

#define KEY4_PIN            (DL_GPIO_PIN_17)
#define KEY4_IOMUX          (IOMUX_PINCM39)
#define KEY4_PORT           (GPIOA)

#define KEY_NONE            0
#define KEY1_PRESS          1
#define KEY2_PRESS          2
#define KEY3_PRESS          3
#define KEY4_PRESS          4

/* ================================================================
 * 循迹传感器 (5路红外, 黑线=1)
 *
 * OUT1=PB19, OUT2=PB17, OUT3=PA16(中心), OUT4=PA14, OUT5=PB20
 * ================================================================ */
#define TK_PORT_A           (GPIOA)
#define TK_PORT_B           (GPIOB)

#define TK_OUT1_PORT        (GPIOB)
#define TK_OUT1_PIN         (DL_GPIO_PIN_19)
#define TK_OUT1_IOMUX       (IOMUX_PINCM45)

#define TK_OUT2_PORT        (GPIOB)
#define TK_OUT2_PIN         (DL_GPIO_PIN_17)
#define TK_OUT2_IOMUX       (IOMUX_PINCM43)

#define TK_OUT3_PORT        (GPIOA)
#define TK_OUT3_PIN         (DL_GPIO_PIN_16)
#define TK_OUT3_IOMUX       (IOMUX_PINCM38)

#define TK_OUT4_PORT        (GPIOA)
#define TK_OUT4_PIN         (DL_GPIO_PIN_14)
#define TK_OUT4_IOMUX       (IOMUX_PINCM36)

#define TK_OUT5_PORT        (GPIOB)
#define TK_OUT5_PIN         (DL_GPIO_PIN_20)
#define TK_OUT5_IOMUX       (IOMUX_PINCM48)

#define TK_SENSOR_COUNT     5
#define TK_WHITE            0
#define TK_BLACK            1

#define REVERSE_SPEED       5
#define REVERSE_DURATION_MS 100   /* 回退持续时间 (抵消惯性前冲) */
#define TRACK_BIAS          10   /* 左右轮不对称补偿 */
#define TRACK_KP            5    /* 循迹 P 增益 */
#define TRACK_KD            3    /* 循迹 D 增益 */

/* ================================================================
 * 舵机 (TIMA0_CCP0 → PA8, 50Hz)
 *
 * prescale=9(÷10), period=63999 → 32M/10/64000 = 50Hz
 * 脉宽: 0°=1600(0.5ms) 90°=4800(1.5ms) 180°=8000(2.5ms)
 * ================================================================ */
#define SERVO_PWM_INST      (TIMA0)
#define SERVO_PWM_CC        DL_TIMER_CC_0_INDEX
#define SERVO_PIN           (DL_GPIO_PIN_8)
#define SERVO_PORT          (GPIOA)
#define SERVO_IOMUX         (IOMUX_PINCM19)
#define SERVO_IOMUX_FUNC    IOMUX_PINCM19_PF_TIMA0_CCP0

#define SERVO_PERIOD        63999
#define SERVO_PRESCALE      9
#define SERVO_CC_MIN        1600
#define SERVO_CC_MID        4800
#define SERVO_CC_MAX        8000
#define SERVO_CC_PER_DEG_X100   3556

/* ================================================================
 * UART (K230 视觉通信, UART0, 115200 8N1)
 *
 * PA28=TX(→K230 RX), PA31=RX(←K230 TX)
 * ================================================================ */
#define UART_INST           UART0
#define UART_IRQN           UART0_INT_IRQn
#define UART_TX_IOMUX       IOMUX_PINCM3              /* PA28 */
#define UART_TX_IOMUX_PF    IOMUX_PINCM3_PF_UART0_TX
#define UART_RX_IOMUX       IOMUX_PINCM6              /* PA31 */
#define UART_RX_IOMUX_PF    IOMUX_PINCM6_PF_UART0_RX
#define UART_BUSCLK_HZ      32000000U
#define UART_BAUDRATE       115200U

/* ================================================================
 * OLED (SSD1306, 软件 I2C)
 *
 * PA0=SDA, PA1=SCL, 地址 0x3C
 * ================================================================ */
#define OLED_SDA_PORT       (GPIOA)
#define OLED_SDA_PIN        (DL_GPIO_PIN_0)
#define OLED_SDA_IOMUX      (IOMUX_PINCM1)
#define OLED_SCL_PORT       (GPIOA)
#define OLED_SCL_PIN        (DL_GPIO_PIN_1)
#define OLED_SCL_IOMUX      (IOMUX_PINCM2)
#define OLED_I2C_ADDR       0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_COLOR_BLACK    0
#define OLED_COLOR_WHITE    1

/* ================================================================
 * 通用宏
 * ================================================================ */
#define ABS(a)              ((a) > 0 ? (a) : -(a))

/* ================================================================
 * 统一初始化接口
 *
 * Board_Init() 依次调用:
 *   SYSCFG_DL_init → Motor_PWM_Init → Encoder_Init → Tracking_Init
 *   → OLED_Init → Key_Init → Servo_PWM_Init → UART_Init
 * ================================================================ */
void Board_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __INIT_H */
