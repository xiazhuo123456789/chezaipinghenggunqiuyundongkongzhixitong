/*
 * encoder.h — Motor Control + Quadrature Encoder API for MSPM0G3507
 *
 * ==================== 完整引脚 ====================
 * TB6612 Motor:
 *   PWMA: PB14  TIMG12_CCP1 (IOMUX_PINCM31, PF=5)  ← 只有这一个 TIMG CCP
 *   AIN1: PB9   GPIO 推挽输出 (IOMUX_PINCM26)
 *   AIN2: PB12  GPIO 推挽输出 (IOMUX_PINCM29)
 *   PWMB: PA7   TIMG7_CCP1 (IOMUX_PINCM14, PF=7)  or TIMG8_CCP0 (PF=4)
 *   BIN1: PB7   GPIO 推挽输出 (IOMUX_PINCM24)
 *   BIN2: PB6   GPIO 推挽输出 (IOMUX_PINCM23)
 *   STBY: 硬件 5V
 *
 * Encoder:
 *   左侧: PB10 (E1A, IOMUX_PINCM27), PB11 (E1B, IOMUX_PINCM28)
 *   右侧: PB4  (E2A, IOMUX_PINCM17), PB5  (E2B, IOMUX_PINCM18)
 *
 * ==================== 设计决策 ====================
 * 1. 方向引脚 (PB9/PB12/PB6/PB7) → SysConfig 配 GPIO, 不动
 * 2. PWM:
 *    PB14 只有 TIMG12_CCP1, PA7 不能共用 TIMG7 因为 PB14 没有 TIMG7 CCP
 *    → 两个独立定时器:
 *         TIMG12 CC1→PB14(PWMA)
 *         TIMG8  CC0→PA7(PWMB)   ← PA7 supports TIMG8_CCP0 (PF=4)
 *    → 完全手动配置, 不依赖 SysConfig PWM 模块
 * 3. 编码器: PB4/PB5 只有 TIMA CCP, 没有 TIMG CCP
 *    → 用 GPIO 数字输入 + 双边沿中断做正交解码, TIMG6 做同步定时器
 */

/* clang-format off */

#ifndef __ENCODER_H
#define __ENCODER_H

#include "init.h"          /* 引脚配置集中于此 */

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format on */

/* ================================================================
 * 引脚宏说明
 *
 * 所有引脚宏 (PWM/方向/编码器/定时器/限幅) 已迁移到 init.h
 * 本文件只保留数据结构 + API 声明
 *
 * 相关宏 (在 init.h 定义):
 *   PWM:   PWM_A_INST, PWM_A_CC1, PWM_B_INST, PWM_B_CC0 ...
 *   方向:  AIN1_SET/AIN2_SET/BIN1_SET/BIN2_SET (依赖 SysConfig)
 *   编码器: ENCODER_PORT, ENC_L_A_PIN, ENC_R_A_PIN ...
 *   定时器: ENC_TIMER_INST, ENC_TIMER_IRQN
 *   限幅:  PWM_MAX, PWM_MIN, TRACK_DEFAULT_SPEED, ABS
 * ================================================================ */


/* ================================================================
 * 编码器数据结构体
 * ================================================================ */
typedef struct {
    int Should_Get_Encoder_Count;
    int Obtained_Get_Encoder_Count;
} Encoder;


/* ================================================================
 * API 声明
 * ================================================================ */

/* 硬件初始化 */
void Motor_PWM_Init(void);        /* 配置 TIMG12(CC1→PB14) + TIMG8(CC0→PA7), 启动 */
void Encoder_Init(void);          /* 编码器 GPIO 输入 + TIMG6 同步定时器 */

/* 编码器同步 (主文件 TIMG6_IRQHandler 调用) */
void Encoder_Sync(void);          /* 原始计数→有效计数, 清零原始计数 */

/* 编码器 */
int  Motor_Get_Encoder(int dir);  /* 0=左, 1=右 */

/* 系统时钟 */
extern volatile uint32_t g_uptime_ms;

/* 电机控制 */
void Motor_Load(int32_t left, int32_t right);  /* 正=前进, 负=后退, 0=停止 */
void Limit(int *a, int *b);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H */
