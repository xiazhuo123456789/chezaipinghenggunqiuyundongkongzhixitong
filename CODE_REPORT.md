# MSPM0G3507 程序代码 (编号3507) 全面解析报告

---

## 一、硬件平台

| 项目 | 说明 |
|------|------|
| **主控芯片** | TI MSPM0G3507 (天猛星 3507 竞赛开发板) |
| **CPU 频率** | 32MHz |
| **电机驱动** | TB6612 双 H 桥 |
| **编码器** | 左右双路正交编码器 (4线: PB10/PB11 + PB4/PB5) |
| **循迹传感器** | 5 路红外反射式 (PB19/PB17/PA16/PA14/PB20) |
| **舵机** | 标准 PWM 舵机 (PA8, 50Hz) |
| **按键** | 4 个 (PA23/PA21/PB18/PA17, 内部上拉, 按下为 0) |
| **显示** | SSD1306 0.96" OLED (PA0=SDA, PA1=SCL, 软件 I2C) |
| **通信** | UART0 (PA28=TX, PA31=RX, 115200) ← K230 视觉模块 |
| **定时器资源** | TIMG6(10ms) / TIMG7(50Hz) / TIMG8(PWMB) / TIMG12(PWMA) / TIMA0(舵机) |

---

## 二、系统架构总览

```
                      ┌─────────────────────────────────┐
                      │           main()                │
                      │  Board_Init() → 8个子系统初始化    │
                      │  for(;;) 主循环:                │
                      │    Pages_HandleKey(Key_GetNum()) │
                      │    Pages_RunActivePage()         │
                      └──────────┬──────────────────────┘
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
     ┌────▼────┐          ┌──────▼──────┐        ┌──────▼──────┐
     │ UART0   │          │  TIMG6 中断  │        │  TIMG7 中断  │
     │ 中断     │          │  (100Hz)    │        │  (50Hz)     │
     │          │          │             │        │             │
     │ K230帧  │          │ g_uptime_ms │        │ Vision_Seq  │
     │ 逐字节   │          │  += 10      │        │  _Tick()    │
     │ 接收     │          │ Encoder_    │        │ 稳球序列    │
     │ \n→解析  │          │  Sync()     │        │ 状态机      │
     │ dx值     │          │ Tracking_   │        │             │
     │ 存入g_dx │          │  Tick()     │        │ Vision_     │
     └─────────┘          │ Key_Tick()  │        │  Tick()     │
                          │ Task_Tick() │        │ PID+舵机    │
                          └─────────────┘        └─────────────┘
```

**并发模型**：三中断并发运行，TIMG6 是实时控制核心（100Hz），TIMG7 负责视觉闭环（50Hz），UART0 按需触发。主循环仅处理按键分发和 OLED 刷新。

---

## 三、源文件详细解析 (逐文件、逐函数)

### 3.1 `init.h` — 全板引脚配置中心 (单一真相源)

**作用**：将项目中所有外设引脚宏集中在一处，避免散落在各模块中导致难以维护。

**结构**：

| 段 | 宏定义 | 说明 |
|----|--------|------|
| 系统时钟 | `CPU_FREQ_HZ = 32000000UL` | 32MHz 总线时钟 |
| 电机 PWM | `PWM_A_INST = TIMG12`, `PWM_A_CC1 → PB14` | 左电机 PWM 10kHz |
| 电机 PWM | `PWM_B_INST = TIMG8`, `PWM_B_CC0 → PA7` | 右电机 PWM 10kHz |
| PWM 限幅 | `PWM_MAX = 200`, `PWM_MIN = -200` | 占空比上下限 |
| 默认速度 | `TRACK_DEFAULT_SPEED = 120` | 参考速度 |
| 方向引脚 | `AIN1_SET(v)`, `AIN2_SET(v)` 等 | 宏内联 GPIO 操作 |
| 编码器 | `ENC_L_A_PIN = PB10` 等 4 路 | 正交编码器输入 |
| 编码器定时器 | `ENC_TIMER_INST = TIMG6` | 100Hz 同步 |
| 按键 | `KEY1_PIN = PA23` 等 4 路 | 内部上拉, 按下=0 |
| 循迹传感器 | `TK_OUT1_PIN = PB19` 等 5 路 | 黑线=1, 白底=0 |
| 循迹参数 | `REVERSE_SPEED=5`, `TRACK_BIAS=10`, `TRACK_KP=5`, `TRACK_KD=3` | PD 控制参数 |
| 舵机 | `SERVO_PWM_INST = TIMA0`, `SERVO_CC_MID = 4800` | 50Hz, 90°中位 |
| UART | `UART_INST = UART0`, 115200 | K230 通信 |
| OLED | `OLED_SDA_PORT = GPIOA`, `OLED_SCL_PIN = PA1` | 软件 I2C |
| 通用宏 | `ABS(a) → ((a)>0 ? (a) : -(a))` | 绝对值 |

**声明**：`void Board_Init(void)` — 统一初始化入口。

---

### 3.2 `init.c` — 统一初始化

**唯一函数**：`Board_Init()`

```c
void Board_Init(void) {
    SYSCFG_DL_init();       // 1. TI 自动生成的系统配置 (时钟/GPIO 等)
    Motor_PWM_Init();       // 2. 双路电机 PWM (TIMG12 CC1 + TIMG8 CC0)
    Encoder_Init();         // 3. 编码器 GPIO 中断 + TIMG6 同步定时器
    Tracking_Init();        // 4. 5 路红外传感器 GPIO 输入
    OLED_Init();            // 5. SSD1306 OLED 软件 I2C
    Key_Init();             // 6. 4 个按键 GPIO 输入(上拉)
    Servo_PWM_Init();       // 7. 舵机 PWM (TIMA0 CCP0, 50Hz)
    UART_Init();            // 8. UART0 115200 + 中断接收
    Page5_TimerInit();      // 9. TIMG7 50Hz 视觉 PID 定时器
    Task_Stop();            // 10. 确保所有任务初始为停止
}
```

**注意**：`Page5_TimerInit()` 必须在 UART/Servo 之后调用（因视觉 PID 依赖这两个模块）。

---

### 3.3 `key.h` / `key.c` — 按键模块

**文件**：`key.h` (17行) / `key.c` (78行)

#### API 声明

| 函数 | 说明 |
|------|------|
| `Key_Init()` | 初始化 4 个按键 GPIO (内部上拉, 滞回使能) |
| `Key_Read()` | 逐位扫描按键, K1>K2>K3>K4 优先级 (同时按下返回最高) |
| `Key_Tick()` | 10ms 周期消抖 + 事件检测 (ISR 调用) |
| `Key_GetNum()` | 主循环读取缓存按键值 + 清零 (非阻塞) |

#### 工作流程

```
Key_Tick() [TIMG6 中断, 每 10ms]
  │
  ├─ Key_Read()  → 读 4 路 GPIO
  │
  ├─ 消抖: 按键变化 → 重置计数; 稳定 → 计数+1
  │
  └─ debounce==19 时:
       ├─ 有按键 → 记录 s_pressed_key
       └─ 无按键 + s_pressed → g_key_val = s_pressed_key (松开触发)
```

**设计意图**：江科大 KeyNum 兼容风格——按下期间不触发，松开瞬间才产生事件，避免长按重复触发。

---

### 3.4 `encoder.h` / `encoder.c` — 电机控制 + 编码器模块

**文件**：`encoder.h` (92行) / `encoder.c` (301行)

#### 数据结构

```c
typedef struct {
    int Should_Get_Encoder_Count;   // 原始计数 (ISR 写入, 高频)
    int Obtained_Get_Encoder_Count; // 有效计数 (Encoder_Sync 同步后)
} Encoder;
```

**双缓冲设计**：`Should_Get` 由 GPIO 中断随时更新 (高频随机写入)，`Obtained_Get` 仅在 10ms 同步时写入，保证读数一致性。

#### 函数详解

| 函数 | 调用者 | 功能 |
|------|--------|------|
| `Motor_PWM_Init()` | Board_Init | 手动配置 TIMG12(CC1→PB14) + TIMG8(CC0→PA7)，10kHz 边沿对齐 PWM |
| `Encoder_Init()` | Board_Init | 4 路编码器 GPIO 输入 + 双边沿中断 + TIMG6@100Hz 同步 |
| `GROUP1_IRQHandler()` | GPIOB 中断 | 正交解码: A 相边沿 → 读 B 电平判方向，±1 累加到 `Should_Get` |
| `Encoder_Sync()` | TIMG6_IRQHandler | `Should_Get` → `Obtained_Get` (右编码器取反, 因机械安装方向), 清零原始计数 |
| `Motor_Get_Encoder(dir)` | 外部读取 | dir=0→左编码器, dir=1→右编码器 |
| `Limit(a,b)` | 无调用者 | 将 PWM 值截断到 [-200, 200] |
| **`Motor_Load(left, right)`** | Set_Car_Speed | **核心接口**: 关中断→设方向引脚→写 PWM CC 值→恢复中断 |

#### Motor_Load 详解

```c
void Motor_Load(int32_t left, int32_t right) {
    __disable_irq();  // 临界区保护 (中断内/外并发安全)

    // 左电机 (PB14): IN1/IN2 设方向 → TIMG12 CC1 = |left|
    if (left > 0)       { AIN1_SET(1); AIN2_SET(0); }   // 正转=前进
    else if (left < 0)  { AIN1_SET(0); AIN2_SET(1); }   // 反转=后退
    else                { AIN1_SET(0); AIN2_SET(0); }   // 刹车

    // 右电机 (PA7): BIN1/BIN2 设方向 → TIMG8 CC0 = |right|
    // (同上逻辑)

    if (!primask) __enable_irq();  // 仅当原状态未关中断时才恢复
}
```

`left` 和 `right` 取值范围 [-200, 200]，正=前进，负=后退。临界区防止 ISR 与主循环并发写方向/PWM 产生中间态桥臂短路。

#### 编码器正交解码 (GROUP1_IRQHandler)

```
A 相边沿触发 → 读取 B 相电平
  B=高 → 正转 → Should_Get_Encoder_Count += 1
  B=低 → 反转 → Should_Get_Encoder_Count -= 1

B 相边沿触发 → 读取 A 相电平
  A=高 → 反转 → Should_Get_Encoder_Count -= 1
  A=低 → 正转 → Should_Get_Encoder_Count += 1
```

这是标准的 4 倍频正交解码。双边沿触发 + 4 通道。

---

### 3.5 `tracking.h` / `tracking.c` — 循迹控制模块

**文件**：`tracking.h` (64行) / `tracking.c` (265行)

#### 数据结构

```c
typedef struct {
    uint8_t is_end;        // 检测到停车线
    uint8_t is_r;          // 检测到右急转弯
    uint8_t is_l;          // 检测到左急转弯
    uint8_t little_end;    // 小圆点检测 (预留)
    uint8_t stop_phase;    // 停车阶段: 0=无 1=刹车 2=回退 3=完成
} track_flags_t;
```

#### 全局变量

| 变量 | 类型 | 用途 |
|------|------|------|
| `tracking_enabled` | `volatile uint8_t` | 1=循迹运行, 0=停止 (主循环写/中断读) |
| `target_speed` | `volatile int32_t` | 动态计算的目标速度 (弯道减速) |
| `track_error` | `volatile int32_t` | 当前偏差距 (正=线偏右, 负=线偏左) |
| `last_track_error` | `volatile float` | 上次偏差 (丢线衰减用) |
| `s_trace_base_speed` | `static int32_t` | 配置基础速度 (默认 18, 由 task_manager 设置) |

**权重表**:
```c
static const int8_t sensor_weights[5] = {
    -5,   // [0] R2 — 最右
    -3,   // [1] R1
     0,   // [2] 中心
    +3,   // [3] L1
    +5,   // [4] L2 — 最左
};
```

#### 函数详解

| 函数 | 调用频率 | 功能 |
|------|----------|------|
| `Tracking_Init()` | 1 次 (Board_Init) | 5 路传感器 GPIO 初始化, 变量清零 |
| **`Tracking_Read()`** | 100Hz | 逐位读 5 个 GPIO→ 存入 `mark_track[5]` 数组, 黑=1/白=0 |
| **`Track_Sensors()`** (static) | 100Hz | 加权偏差计算 + 动态速度 + 停车检测 + 直角弯检测 |
| **`Tracking_Tick()`** | 100Hz (TIMG6) | **主控函数**: 偏差计算 → 停车序列处理 → PD 控制 → `Set_Car_Speed` |
| `Set_Car_Speed(L,R)` | 100Hz | 限幅后调用 `Motor_Load` |
| `Tracking_Enable(en)` | 按键触发 | 启动/停止循迹 |
| `Tracking_Reset()` | 启动时 | 清空所有循迹标志 |
| `Tracking_IsStopped()` | Task_Tick | 返回自然停车线触发 (非用户中止) |

#### 核心算法详解

**1. Track_Sensors — 加权偏差**（`tracking.c:96-147`）

```
遍历 5 路传感器:
  mark_track[i] == 黑 → total_error += sensor_weights[i]; active_sensors++

丢线处理 (全部白=飞出赛道):
  total_error = last_track_error * 0.9  (指数衰减回到 0)

输出: track_error = (int32_t)total_error
```

**2. 动态目标速度**（`tracking.c:123-130`）

```
target_speed = base_speed - (5 * |track_error|) / 10
                 clamped to [10, base_speed + 6]
```

弯道偏差越大速度越低，保证过弯不飞线。

**3. 停车检测**（`tracking.c:132-140`）

```
条件: mark_track[0,1,2,3] 同时为黑 → stop_phase = 1 (触发刹车)
```

即右侧 4 个传感器同时碰到黑线（停车线/斑马线），触发停车。

**4. 直角弯检测**（`tracking.c:142-146`）

```
mark_track[0] && mark_track[1] → is_r = 1 (线右拐)
mark_track[3] && mark_track[4] → is_l = 1 (线左拐)
```

**5. Tracking_Tick — PD 电机控制**（`tracking.c:207-226`）

```
偏差死区: |track_error| ≤ 3 → corr = 0 (直走不抖)
否则:
  err_f = (float)track_error
  d_term = TRACK_KD * (err_f - s_prev_track_err)   // D=3
  s_prev_track_err = err_f
  corr = track_error * TRACK_KP + d_term           // P=5

base = s_trace_base_speed  (如 18/25/35)
L = base - corr + TRACK_BIAS  (BIAS=10, 左轮补偿)
R = base + corr
Set_Car_Speed(L, R)  → Motor_Load
```

**6. 精准停车三阶段**（`tracking.c:175-198`）

```
阶段1: 立即刹车 → 记录时间戳 → 进入阶段2
阶段2: 回退 5 速度 × 100ms → 抵消惯性冲 → 进入阶段3
阶段3: 停止循迹 (tracking_enabled=0, is_end=1), 清零所有变量
```

---

### 3.6 `servo.h` / `servo.c` — 舵机模块

**文件**：`servo.h` (44行) / `servo.c` (131行)

#### 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 定时器 | TIMA0 CCP0 → PA8 | 16 位定时器 |
| 频率 | 50Hz (20ms 周期) | 标准舵机 |
| 周期 | 63999 tick | 32M/10/64000 = 50Hz |
| 精度 | ~0.028°/tick | 312.5ns/tick |
| 0°→CC=1600 (0.5ms) | 90°→CC=4800 (1.5ms) | 180°→CC=8000 (2.5ms) |

#### 函数详解

| 函数 | 说明 |
|------|------|
| `Servo_PWM_Init()` | TIMA0 边沿对齐 PWM, 初始 90°, IOMUX PA8 |
| `Servo_Set_Angle(0..180)` | 定点乘法: `CC = angle * 3556/100 + 1600`, 无浮点 |
| `Servo_Set_AngleF(float)` | 浮点版, 支持小数度 (如 81.5°) |
| `Servo_Set_Pulse(cc)` | 原始 CC 值写入 (限幅 1600~8000) |

---

### 3.7 `oled.h` / `oled.c` / `oledfont.h` — OLED 显示模块

**文件**：`oled.h` (14行) / `oled.c` (104行) / `oledfont.h` (427行, 含汉字字模)

#### 接口

| 函数 | 说明 |
|------|------|
| `OLED_Init()` | SSD1306 初始化序列 (27 个命令字节) |
| `OLED_Clear()` | 清空缓冲 + 刷新 |
| `OLED_ShowString(x, page, str, size)` | 6×8 字符 (size 参数保留但未用) |
| `OLED_ShowNum(x, page, num, len, size)` | 数字格式化后显示 |
| `OLED_Refresh()` | 全缓冲 1024 字节 → I2C 传输 (耗时约 345ms) |
| `OLED_ShowPage(1..5)` | 显示菜单页标题 |

#### 软件 I2C 实现

```
sda_1/sda_0/scl_1/scl_0 ← 直接 GPIO 操作宏
start()/stop()/wb(byte) ← 位冲击, delay_cycles(600) 做时序
                          ACK 位: 读 DA 引脚但丢弃结果 (无错误检测)
```

**性能瓶颈**：`OLED_Refresh()` 输出 8 页 × 128 字节 = 1024 字节，软件 I2C 阻塞约 345ms。这就是所有实时控制必须放在中断里的原因。

**字模**：`oledfont.h` 包含 4 种字号 (6×8 / 12×6 / 16×8 / 24×12 ASCII) + 汉字字模 (16×16/24×24/32×32/64×64)。

---

### 3.8 `uart.h` / `uart.c` — UART 通信模块

**文件**：`uart.h` (36行) / `uart.c` (106行)

**协议**：K230 视觉模块发送 ASCII 帧 `"dx=+90\n"` / `"dx=-30\n"`

#### 接口

| 函数 | 调用者 | 说明 |
|------|--------|------|
| `UART_Init()` | Board_Init | UART0 115200 8N1, RX 中断接收 |
| `UART_Tick()` | UART0_IRQHandler | 逐字节接收→行缓冲 `\n` 触发解析 |
| `UART_Get_Dx()` | 视觉 PID | 读取最新 dx 值 (像素偏差距) |

#### 解析引擎

```
UART_Tick():
  while (!RX FIFO 空)
    c = 接收字节
    if c == '\n' → parse_line(缓冲, 长度); 长度=0
    elif c != '\r' → 缓冲[长度++] = c
    溢出 → 长度=0 (丢弃整帧, 重新同步)

parse_line():
  跳过可选 "dx=" 前缀
  解析符号 (+/-)
  逐数字计算: val = val*10 + digit
  结果: g_dx = sign * val
  g_last_rx_ms = g_uptime_ms (记录时间戳, 用于超时检测)
```

---

### 3.9 `pages.h` / `pages.c` — 页面/菜单系统 + 视觉 PID

**文件**：`pages.h` (63行) / `pages.c` (392行)

这是最复杂的模块，融合了**页面路由**和**视觉 PID 控制**。

#### 表驱动页面路由

```c
typedef struct {
    uint8_t      next_page;  // KEY4 跳到哪页
    Page_OnKey   on_key;     // 按键回调
    Page_OnRun   on_run;     // 每帧活跃回调
    Page_OnLeave on_leave;   // 离开清理回调
} Page_Entry;

static const Page_Entry s_page_table[5] = {
    { 2, Page1_OnKey, Page1_Run, Page1_OnLeave },  // 1=Hello
    { 3, Page2_OnKey, Page2_Run, Page2_OnLeave },  // 2=循迹
    { 4, Page3_OnKey, Page3_Run, Page3_OnLeave },  // 3=舵机
    { 5, Page4_OnKey, Page4_Run, Page4_OnLeave },  // 4=钢球
    { 1, Page5_OnKey, Page5_Run, Page5_OnLeave },  // 5=视觉
};
```

#### 按键分发

```
Pages_HandleKey(key):
  if key == KEY_NONE → return
  if key == KEY4_PRESS:            // KEY4 = 切页
    当前页.on_leave()               // 清理 (如停循迹、舵机归位)
    切到 next_page
    OLED_ShowPage(新页号)
    return
  当前页.on_key(key)                // KEY1/2/3 → 页面自己处理
```

#### 各页面行为

| 页面 | KEY2 | KEY3 | KEY4 | 运行时 |
|------|------|------|------|--------|
| 1 Hello | (忽略) | (忽略) | →页面2 | 无 |
| 2 循迹 | Task_Start(TASK_2) 启动循迹 | Task_Stop() 停车 | →页面3 | Task_DisplayStatus() |
| 3 舵机 | 角度 +5° | 角度 -5° | →页面4 | 首次初始化 90°, 变化时刷新 OLED |
| 4 钢球 | Task_Start(TASK_3) 启动摆杆 | 复位 | →页面5 | Task_DisplayStatus() |
| 5 视觉 | Task_Start(TASK_4) | Task_Stop | →页面1 | 显示 dx/vel/stay 实时数据 |

#### 视觉 PID (Vision_Tick) — 50Hz ISR

这是程序最复杂的算法：

```
Vision_Tick() [TIMG7 中断, 每 20ms]:

STEP 0: 状态检查
  if s_vision_active==0 → 标志 was_active=0, return

STEP 1: 跳变检测 (0→1)
  if !was_active:
    重置 s_vision_last_rx_ms / s_vision_integral / s_vision_prev_err / s_vision_last_pos
    was_active = 1; g_vision_ball_vel = 0
    return (跳变帧不执行 PID, 等下一帧获取新鲜 dx)

STEP 2: 超时保护
  if 5000ms 未收到 K230 帧 → 标志 timeout=1, 停任务, 舵机回 90°, return

STEP 3: 速度估计 (一阶低通)
  dx = UART_Get_Dx()
  ball_pos = (float)dx
  vel_raw = ball_pos - s_vision_last_pos       // 帧差 = 像素/20ms
  vel_now = 0.6*g_vision_ball_vel + 0.4*vel_raw  // α=0.4 低通
  g_vision_ball_vel = vel_now * 100             // 定标 ×100 存 int16
  s_vision_last_pos = ball_pos

STEP 4: 死区检测
  error = ball_pos - g_vision_target_pos
  if |error|≤5px && |vel_now|<1.0px/frame:
    stay_cnt++; 舵机回 90°; return

STEP 5: PID 计算
  P: p_out = Kp * error                    (Kp=0.12)
  D: d_out = Kd * (error - s_prev_err)     (Kd=0.36)
  I: if |error|<30 → 积分 += error*0.3     (条件积分, 避免饱和)
     else → 积分 *= 0.5                    (误差大时衰减积分)
     积分限幅 [-200, 200]
     i_out = Ki * integral                 (Ki=0.002)

STEP 6: 输出
  correction = P+I+D, 限幅 [-20, 20]
  angle = 90° - correction, 限幅 [15°, 165°]
  Servo_Set_AngleF(angle)
```

**PID 参数** (移植自 Keil 省赛 Q3_Servo_Control_New):

| 系数 | 值 | 作用 |
|------|-----|------|
| Kp | 0.12 | 位置比例: 球偏右 → 舵机左倾 |
| Kd | 0.36 | 速度微分: 抑制震荡 |
| Ki | 0.002 | 积分: 消除静差 |

---

### 3.10 `task_manager.h` / `task_manager.c` — 任务调度器 + 稳球序列状态机

**文件**：`task_manager.h` (47行) / `task_manager.c` (300行)

这是**任务调度核心**，也是**稳球序列状态机**的实现。

#### 任务类型

```c
typedef enum {
    TASK_2 = 2,  // 只循迹 (speed=18)
    TASK_3 = 3,  // 钢珠 0→+80→-80 (稳球, 不循迹)
    TASK_4 = 4,  // 循迹+稳球 低速 (speed=18)
    TASK_5 = 5,  // 循迹+稳球 中速 (speed=25)
    TASK_6 = 6,  // 循迹+稳球 高速 (speed=35)
} TaskID;
```

#### 任务配置表

```c
static const TaskConfig s_task_cfg[] = {
    [TASK_2] = { 1, 0, 18, NULL,         0 },  // 循迹=是, 稳球=否, 速度=18
    [TASK_3] = { 0, 1, 0,  s_seq_task3,  2 },  // 稳球=是, 序列=[+80, -80]
    [TASK_4] = { 1, 1, 18, NULL,         0 },
    [TASK_5] = { 1, 1, 25, NULL,         0 },
    [TASK_6] = { 1, 1, 35, NULL,         0 },
};
```

#### 调度流程

```
Task_Start(id):
  Task_Stop()          // 先停当前任务
  查表 → 获取 has_track / has_vision / track_speed / vision_seq
  if has_track → Tracking_Reset() + SetBaseSpeed(配置速度) + Enable(1)
  if has_vision → Vision_SetSequence() + Vision_Enable(1)

Task_Tick() [TIMG6, 每 10ms]:
  任务计时器 += 10
  if TASK_2 → Tracking_IsStopped() → 自动停任务

Task_Stop():
  Tracking_Enable(0) + Vision_Enable(0)
  清理所有状态
```

#### 稳球序列状态机 (Vision_Seq_Tick)

这是任务 3 的核心——控制钢球摆杆按预定轨迹运动。

```
状态: IDLE → MOVE → HOLD → MOVE → HOLD → ... → DONE

Vision_Seq_Tick() [TIMG7, 50Hz]:
  switch(s_vs_state):
    VS_IDLE:
      首次激活 → 进入 VS_MOVE, 目标 = sequence[0]

    VS_MOVE:
      target = sequence[current_idx]       // 当前位置目标 (如 +80 像素)
      diff = target - g_vision_target_pos  // 差距
      if |diff| > 10 → target_pos += ±10   // 渐进 (±10 像素/帧, 50Hz)
      else           → target_pos = target // 到位
      if 到位 && stay_cnt≥30 (600ms 稳球):
        进入 VS_HOLD, 记录时间戳

    VS_HOLD:
      保持 1000ms (VS_HOLD_TIME)
      时间到 → current_idx++, 下一个 MOVE
      所有步完成 → VS_DONE, target_pos=0 (回中心)

    VS_DONE:
      保持中心, 不再动作
```

**序列**: `s_seq_task3[] = { 80, -80 }`  
→ 钢球从中心(0) → 右偏 80 像素 → 稳 → 左偏 80 像素 → 稳 → 回中心 → 完成

---

### 3.11 `empty_mspm0g3507.c` — 主程序

**文件**：`empty_mspm0g3507.c` (61行)

```c
int main(void) {
    Board_Init();           // 全部硬件初始化
    OLED_ShowPage(1);       // 显示 Hello World

    for (;;) {
        Pages_HandleKey(Key_GetNum());   // 按键→页面分发
        Pages_RunActivePage();           // 当前页面活跃任务
    }
}
```

**三个中断服务函数**:

| ISR | 频率 | 调用链 |
|-----|------|--------|
| `TIMG6_IRQHandler` | 100Hz | `g_uptime_ms+=10` → `Encoder_Sync` → `Tracking_Tick` → `Key_Tick` → `Task_Tick` → 清中断 |
| `TIMG7_IRQHandler` | 50Hz | `Vision_Seq_Tick()` → `Vision_Tick()` → 清中断 |
| `UART0_IRQHandler` | 按需 | `UART_Tick()` (逐字节接收解析) |

---

## 四、完整运行流程示例

### 场景 A: 纯循迹 (TASK_2)

```
1. 上电 → Board_Init → OLED 显示 "Hello World"
2. 按键 KEY4 → 切换到页面 2 "TRACKING"
3. 按键 KEY2 → Task_Start(TASK_2)
     → Tracking_SetBaseSpeed(18)
     → Tracking_Enable(1): tracking_enabled = 1
4. TIMG6 每 10ms:
     Tracking_Tick():
       Track_Sensors() → 读 5 路传感器
       → 计算加权偏差 track_error (如线偏右 → 正值)
       → 计算 target_speed (弯道减速)
       → PD 控制: L = 18 - corr + 10, R = 18 + corr
       → Motor_Load(L, R) → 调整差速左转
5. 车辆沿黑线前进...
6. 到达停车线:
     Track_Sensors() → mark_track[0..3] 全黑 → stop_phase=1
     Tracking_Tick():
       阶段1: 刹车 (PWM=0)
       阶段2: 回退 100ms (PWM=-5)
       阶段3: 停 (tracking_enabled=0, is_end=1)
7. Task_Tick() → Tracking_IsStopped()=true → Task_Stop()
8. OLED 显示 "T2: STOP"
```

### 场景 B: 钢球摆杆 (TASK_3)

```
1. 页面 4, KEY2 → Task_Start(TASK_3)
     → Vision_SetSequence([80, -80], 2)
     → Vision_Enable(1): s_vs_active=1, s_vs_state=IDLE

2. TIMG7 每 20ms:
     Vision_Seq_Tick():                 // 先更新目标位置
       IDLE→MOVE: target_pos 从 0 渐进+10→+80
       hold: stay_cnt≥30 → VS_HOLD, 保持 1000ms
       然后 target_pos +80→-80, 再 hold
       最后 DONE: target_pos=0

     Vision_Tick():                     // 再执行 PID
       跳变检测 (首次) → 重置状态, skip
       读取 K230 dx → 计算球位置
       死区判断 → 需要移 ?
       PID: error = ball_pos - target_pos → 计算舵机角度
       Servo_Set_AngleF(angle)

3. 钢球在舵机控制下到达目标位置 → wait 600ms判稳 → 下一目标
4. 序列完成 → 舵机回 90°
```

### 场景 C: 循迹+稳球 中速 (TASK_5)

```
1. 页面 5, KEY2 → Task_Start(TASK_4/TASK_5/TASK_6)
     → Tracking_Enable(1), speed=25
     → Vision_Enable(1)

2. TIMG6: Tracking_Tick 正常循迹走线 (speed=25)
3. TIMG7: Vision_Tick 始终稳球 (target=0 保持中心)
4. 车辆边走线边稳定球 → 到达停车线后自动停止
```

---

## 五、模块依赖关系

```
main() 依赖:
  ├── init.h → init.c
  ├── encoder.h → encoder.c (Motor_PWM_Init, Encoder_Init, Encoder_Sync, Motor_Load)
  │   └── tracking.h → tracking.c (依赖 encoder.h 的 Motor_Load)
  ├── key.h → key.c
  ├── oled.h → oled.c + oledfont.h
  ├── servo.h → servo.c
  ├── uart.h → uart.c
  ├── pages.h → pages.c (依赖 tracking/oled/key/servo/uart/task_manager)
  │   ├── Vision_Tick() → 直接操作 Servo + UART + Task_Stop
  │   └── 页面回调 → Task_Start/Task_Stop → 控制 tracking + vision
  └── task_manager.h → task_manager.c
      ├── 依赖 tracking.h (Tracking_Enable/SetBaseSpeed/IsStopped)
      └── Vision_Seq_Tick() ← TIMG7 调用
```

**循环依赖**：

```
task_manager.c → extern Vision_Enable (定义在 task_manager.c 自身!)
pages.c      → extern Vision_Seq_Tick (定义在 task_manager.c)
```

这不是真正的循环依赖，而是 `extern` 前向声明用来指示调用方向。

---

## 六、时间预算分析

| 中断 | 周期 | 执行内容 | 预估耗时 |
|------|------|----------|----------|
| TIMG6 | 10ms | Encoder_Sync + Tracking_Tick + Key_Tick + Task_Tick | <1ms |
| TIMG7 | 20ms | Vision_Seq_Tick + Vision_Tick (浮点 PID) | <2ms |
| UART0 | 按需 | 逐字节解析 | <0.1ms |
| GPIOB (编码器) | 按需 | 正交解码 | <0.1ms |
| main 循环 | 空闲 | Pages_HandleKey + Pages_RunActivePage | 依赖 OLED 刷新 (~345ms) |

**关键**：所有实时控制 (循迹、视觉、按键扫描) 都在中断中执行，不依赖主循环速度。主循环仅做按键分发和 OLED 显示，即使 OLED 刷新阻塞 345ms 也不影响实时性。

---

## 七、设计特点总结

1. **三中断分层架构** — 100Hz 实时控制层、50Hz 视觉层、UART 通信层，各司其职
2. **双表驱动** — 页面表 (`s_page_table`) + 任务配置表 (`s_task_cfg`)，新增功能只需填表
3. **单一真相源** — 所有引脚宏集中 `init.h`，换硬件只改一文件
4. **双缓冲编码器** — `Should_Get` (高频 ISR 写入) / `Obtained_Get` (同步时读取) 保证数据一致性
5. **浮点 PID + 定点舵机** — 视觉 PID 用浮点精算（非热点路径），舵机输出用定点避免浮点开销
6. **软件 I2C OLED** — 节省硬件 I2C 资源，代价是刷新时阻塞 345ms，但中断不受影响
7. **死区 + 条件积分** — 循迹偏差死区 (±3) 防止微调抖动，视觉积分在误差大时衰减避免饱和
8. **并发安全** — `Motor_Load` 用关中断保护方向+PWM 原子性，`volatile` 保证中断/主循环可见性
