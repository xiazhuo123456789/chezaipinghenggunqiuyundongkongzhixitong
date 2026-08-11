/*
 * uart.h — K230 视觉模块 UART 通信 API (MSPM0G3507)
 *
 * 引脚: PA28 → UART0_TX (→ K230 RX, 调试用)
 *       PA31 → UART0_RX (← K230 TX, 核心数据)
 * 波特率: 115200, 8N1
 *
 * 协议: ASCII 帧 "dx=+90\n" / "dx=-30\n"
 *   - 前缀 "dx="
 *   - 带符号整数 (正号可省略)
 *   - '\n' 结束
 */

#ifndef __UART_H
#define __UART_H

#include "ti_msp_dl_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 UART0 (115200, 8N1, 中断接收) */
void UART_Init(void);

/* UART 接收处理 (主文件 UART0_IRQHandler 调用) */
void UART_Tick(void);

/* 获取最新 dx 值 (主循环调用, 线程安全) */
int UART_Get_Dx(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_H */
