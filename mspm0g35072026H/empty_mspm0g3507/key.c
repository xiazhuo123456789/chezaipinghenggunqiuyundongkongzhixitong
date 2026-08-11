#include "key.h"

void Key_Init(void)
{
    DL_GPIO_initDigitalInputFeatures(KEY1_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY2_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY3_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY4_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
}

/* 逐位扫描按键 (内部上拉, 按下=0, 同时按下返回优先级最高的键: K1>K2>K3>K4) */
uint8_t Key_Read(void)
{
    if (DL_GPIO_readPins(KEY1_PORT, KEY1_PIN) == 0) return KEY1_PRESS;
    if (DL_GPIO_readPins(KEY2_PORT, KEY2_PIN) == 0) return KEY2_PRESS;
    if (DL_GPIO_readPins(KEY3_PORT, KEY3_PIN) == 0) return KEY3_PRESS;
    if (DL_GPIO_readPins(KEY4_PORT, KEY4_PIN) == 0) return KEY4_PRESS;
    return KEY_NONE;
}

/* ================================================================
 * 江协科技风格按键实现
 *
 * 工作原理:
 *   1. Key_Tick() 在 10ms 中断里调用, 不断扫描+消抖
 *   2. 检测到“按下→松开”的完整事件后, 缓存按键值到 g_key_val
 *   3. Key_GetNum() 在主循环调用, 读取并清零 g_key_val
 *
 * 事件触发模式 (松开才返回):
 *   - 按下期间: debounce 从 0 加到 19, 期间不触发
 *   - 松开瞬间: 检测到 key=NONE 且上次有按键 → 触发 g_key_val = last_key
 *   - 优势: 避免长按重复触发, 与江科大 KeyNum 语义一致
 * ================================================================ */
static volatile uint8_t g_key_val  = KEY_NONE;  /* 缓存的按键值 */
static uint8_t  s_last_key  = KEY_NONE;          /* 上次扫描到的按键 */
static uint8_t  s_debounce  = 0;                 /* 消抖计数器 (0~20) */
static uint8_t  s_pressed   = 0;                 /* 已按下标志 (等松开) */
static uint8_t  s_pressed_key = KEY_NONE;        /* 已按下的键号 */

void Key_Tick(void)
{
    uint8_t key = Key_Read();

    /* 状态变化 → 重置消抖计数 */
    if (key != s_last_key) {
        s_debounce = 0;
        s_last_key = key;
    } else {
        if (s_debounce < 20) s_debounce++;
    }

    /* 消抖稳定后处理 */
    if (s_debounce == 19) {
        if (key != KEY_NONE) {
            /* 按下稳定 → 记录已按下 */
            s_pressed     = 1;
            s_pressed_key = key;
        } else {
            /* 松开稳定 → 如果之前有按下, 触发按键事件 */
            if (s_pressed) {
                g_key_val    = s_pressed_key;
                s_pressed    = 0;
                s_pressed_key = KEY_NONE;
            }
        }
    }
}

uint8_t Key_GetNum(void)
{
    uint8_t val = g_key_val;
    g_key_val = KEY_NONE;   /* 读后清零, 避免重复处理 */
    return val;
}
