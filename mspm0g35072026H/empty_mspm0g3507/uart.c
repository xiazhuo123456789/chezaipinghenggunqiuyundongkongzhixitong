/*
 * uart.c — K230 视觉模块 UART 通信实现
 *
 * 硬件: UART0 (PA28 TX, PA31 RX), 115200 8N1
 * 时钟: BUSCLK 32MHz (MSPM0G3507 默认)
 *   IBRD=17, FBRD=23 → 实际 115207 bps (误差 0.006%)
 *
 * 解析: 行缓冲 + '\n' 触发解析, 匹配 "dx=" 前缀提取带符号整数
 *
 * 引脚宏来自 init.h (UART_INST, UART_TX_IOMUX, UART_RX_IOMUX 等)
 */

#include "uart.h"
#include "init.h"

#define UART_BUF_SIZE       16    /* "dx=-XXX" 最长 8 字节, 16 足够 */

static char     g_rx_buf[UART_BUF_SIZE];
static uint8_t  g_rx_len = 0;
static volatile int      g_dx          = 0;
static volatile uint32_t g_last_rx_ms  = 0;

extern volatile uint32_t g_uptime_ms;  /* encoder.c 定义, 10ms 递增 */

/* parse one line: "dx=+90\n" → 90, "+90\n" → 90, "-30\n" → -30 */
static void parse_line(const char *s, uint8_t n)
{
    if (n == 0) return;

    uint8_t i = 0;

    /* skip optional "dx=" prefix */
    if (n >= 3 && s[0] == 'd' && s[1] == 'x' && s[2] == '=') {
        i = 3;
    }

    int sign = 1;
    if (i < n && s[i] == '+')      { i++; }
    else if (i < n && s[i] == '-') { sign = -1; i++; }

    int val = 0;
    uint8_t got_digit = 0;
    for (; i < n; i++) {
        if (s[i] >= '0' && s[i] <= '9') {
            val = val * 10 + (s[i] - '0');
            got_digit = 1;
        } else {
            break;
        }
    }
    if (!got_digit) return;

    g_dx = sign * val;
    g_last_rx_ms = g_uptime_ms;
}

void UART_Init(void)
{
    DL_UART_reset(UART_INST);
    DL_UART_enablePower(UART_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_UART_ClockConfig ck = {
        .clockSel    = DL_UART_CLOCK_BUSCLK,
        .divideRatio = DL_UART_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_setClockConfig(UART_INST, &ck);

    DL_UART_Config cfg = {
        .mode       = DL_UART_MODE_NORMAL,
        .direction  = DL_UART_DIRECTION_TX_RX,
        .flowControl= DL_UART_FLOW_CONTROL_NONE,
        .parity     = DL_UART_PARITY_NONE,
        .wordLength = DL_UART_WORD_LENGTH_8_BITS,
        .stopBits   = DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(UART_INST, &cfg);

    DL_UART_configBaudRate(UART_INST, UART_BUSCLK_HZ, UART_BAUDRATE);

    /* IOMUX: PA28→UART0_TX (output), PA31→UART0_RX (input) */
    DL_GPIO_initPeripheralOutputFunction(UART_TX_IOMUX, UART_TX_IOMUX_PF);
    DL_GPIO_initPeripheralInputFunction(UART_RX_IOMUX, UART_RX_IOMUX_PF);

    /* enable RX interrupt: trigger on RX (single-byte @ 115200) */
    DL_UART_enableInterrupt(UART_INST, DL_UART_INTERRUPT_RX);
    NVIC_ClearPendingIRQ(UART_IRQN);
    NVIC_EnableIRQ(UART_IRQN);

    DL_UART_enable(UART_INST);
}

/* read-current-dx accessor (main loop: non-blocking) */
int UART_Get_Dx(void)              { return g_dx; }

/* UART_Tick: 逐字节接收 + 行缓冲 + \n 触发解析 (主文件 UART0_IRQHandler 调用) */
void UART_Tick(void)
{
    switch (DL_UART_getPendingInterrupt(UART_INST)) {
    case DL_UART_IIDX_RX:
        while (!DL_UART_isRXFIFOEmpty(UART_INST)) {
            char c = (char)DL_UART_receiveData(UART_INST);
            if (c == '\n') {
                parse_line(g_rx_buf, g_rx_len);
                g_rx_len = 0;
            } else if (c != '\r') {
                if (g_rx_len < UART_BUF_SIZE - 1) {
                    g_rx_buf[g_rx_len++] = c;
                } else {
                    g_rx_len = 0;  /* overflow: discard frame, resync */
                }
            }
        }
        break;
    default:
        break;  /* OE/BE/PE/FE: reading IIDX already cleared */
    }
}
