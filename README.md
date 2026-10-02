# 车载平衡滚球运动控制系统 H 题

> 2026 年全国大学生电子设计竞赛 H 题完整实现方案。

## 📋 项目简介

本项目是 **2026 年电赛 H 题「车载平衡滚球运动控制系统」** 的车载主控端代码，运行在 **TI MSPM0G3507**（天猛星 3507 开发板）上，配合 **K230 视觉模块** 实现小车循迹、钢球平衡、动态稳球等复合控制任务。

### 控制架构：双环 PD 控制 + 三中断分层

#### 1. 循迹控制环（Tracking PD，10ms 周期）

**传感器配置：5 路红外寻迹模块**
```
排列：[R2] [R1] [中心] [L1] [L2]
权重： -5   -3    0     +3   +5
```

**加权偏差计算：**
```c
track_error = Σ(sensor[i] × weight[i]) / active_sensors
```
- 传感器检测到黑线时贡献权重
- 多个传感器同时触发时取平均，抑制噪声
- 丢线时使用历史偏差的 0.9 倍衰减（last_error × 0.9）

**PD 控制器：**
```c
output = Kp × error + Kd × (error - prev_error)
left_pwm  = base_speed - output + bias
right_pwm = base_speed + output
```

**参数配置：**
- `TRACK_KP = 1.5`：比例增益（越大转向越灵敏，过大易震荡）
- `TRACK_KD = 0.36`：微分增益（平滑转向，抑制振荡）
- `TRACK_BIAS = 0`：左右电机补偿（修正车身不对称）

**调试策略：**
1. **只开 P**：调节 `Kp` 使车能跟住黑线但可能有轻微摆动
2. **加 D**：增加 `Kd` 平滑转向动作，消除摆动
3. **死区处理**：当 `|error| <= 3` 时（1-2 个传感器在线），修正量置 0，直走

**与速度环的关系：**
循迹 PD 直接输出 PWM 差速指令，不经过独立的速度闭环。基础速度 `base_speed` 由外部任务管理器设定（如任务 1 设 18，任务 2 设 25）。

**丢线保护：**
- 所有传感器都检测不到黑线时，使用历史偏差的 0.9 倍继续输出
- 连续丢线超过 500ms 触发紧急停车

**精准停车（三阶段）：**
```
阶段 1：检测到停车线（4 个传感器同时黑） → 立即刹车
阶段 2：回退 150ms（PWM = -15）            → 抵消惯性 + 万向轮右偏
阶段 3：最终停车（PWM = 0）                → 关闭循迹标志
```

#### 2. 视觉稳球环（Vision PID，50Hz 周期）

**视觉数据接收：**
```c
// K230 通过 UART0 发送（115200 波特率）
// 协议格式："dx=±123\n"
// dx：钢球相对平台中心的偏移量（像素）
//     正值 = 球在右侧，负值 = 球在左侧
```

**PID 控制器：**
```c
error = target_position - ball_position
integral += error × dt
derivative = (error - prev_error) / dt
servo_output = Kp × error + Ki × integral + Kd × derivative
```

**参数配置：**
- `VISION_KP = 0.12`：比例增益（球偏离越远，舵机转动越大）
- `VISION_KI = 0.002`：积分增益（消除平台轻微倾斜导致的稳态偏差）
- `VISION_KD = 0.36`：微分增益（预测球的运动趋势，提前调整）

**50Hz 周期的意义：**
- K230 视觉处理帧率约 50fps（20ms/帧）
- 舵机 PWM 标准刷新率 50Hz
- 钢球动力学频率较低（< 10Hz），50Hz 采样足够

**积分抗饱和：**
```c
if (integral > MAX_INTEGRAL)  integral = MAX_INTEGRAL;
if (integral < -MAX_INTEGRAL) integral = -MAX_INTEGRAL;
```
防止积分累积过大导致舵机输出失控。

**调试策略：**
1. **只开 P**：球能回到中心附近但可能有稳态偏差（停在 ±10px）
2. **加 I**：消除稳态偏差，球精确居中（注意防止积分饱和）
3. **加 D**：球运动时提前预判，减少超调和振荡

**与循迹的协同：**
- 循迹控制车身位置（10ms 快速响应）
- 视觉控制平台姿态（50Hz 精确稳球）
- 两者通过 `volatile` 全局标志协调启停

#### 3. 三中断分层时序设计

```
TIMG6（10ms）：  循迹 PD 计算 + 电机输出 + 按键扫描
TIMG7（20ms）：  视觉 PID 计算 + 舵机输出（50Hz）
UART0（异步）：  K230 视觉数据接收（非阻塞）
主循环（慢速）： OLED 刷新 + 菜单交互
```

**为什么分三层中断？**
1. **循迹需要快速响应**：10ms 周期确保车不跑偏
2. **视觉处理相对慢**：20ms 周期（50Hz）匹配摄像头帧率
3. **串口异步接收**：不阻塞其他任务，数据到达立即处理

**中断优先级：**
```
UART0（高）   → 视觉数据实时性要求高，丢帧影响大
TIMG6（中）   → 循迹控制需要优先保证
TIMG7（低）   → 舵机调整允许轻微延迟
```

**并发安全保护：**
```c
// 所有跨中断共享的变量都声明为 volatile
volatile uint8_t tracking_enabled = 0;
volatile int32_t track_error = 0;
volatile int32_t ball_position = 0;
```

**时序配合示例：**
```
时刻 0ms:   TIMG6 触发 → 读传感器 → 计算偏差 → 输出电机 PWM
时刻 5ms:   UART 接收 → "dx=+45\n" → 更新 ball_position
时刻 10ms:  TIMG6 触发 → 循迹控制
时刻 20ms:  TIMG7 触发 → 读 ball_position → PID 计算 → 舵机输出
时刻 30ms:  TIMG6 触发 → 循迹控制
时刻 40ms:  TIMG7 触发 → 稳球控制
```

循迹和视觉控制完全解耦，互不干扰。

系统核心采用**三中断分层架构** + **双表驱动设计**，模块间通过 Tick 函数解耦，**主文件仅 60 行**。

## ✨ 核心功能

- 🚗 **5 路红外循迹**（PD 控制 + 三阶段精准停车）
- 📷 **K230 视觉接收**（UART 115200，`dx=±XXX` 协议）
- ⚖️ **钢球位置闭环 PID 控制**（舵机 50Hz 调节）
- 📋 **6 任务统一调度**（表驱动配置，一键启动）
- 🖥️ **OLED 菜单系统**（5 页面表驱动切换）
- 🔒 **全局 volatile + 临界区保护**（并发安全）

## 🛠️ 技术栈

- **主控芯片**：TI MSPM0G3507（Cortex-M0+，80MHz）
- **视觉模块**：K230（嘉楠科技，双核 RISC-V）
- **开发环境**：Code Composer Studio (CCS) 12.6+ 或 Theia 1.x
- **SDK**：MSPM0G3507 SDK v2.01+
- **编译器**：TI Clang for ARM
- **调试器**：XDS110（板载）或 J-Link

## 📐 软件架构

### 整体架构：三中断分层 + 双表驱动

**三中断分层时序详解：**

#### TIMG6 中断（10ms 周期，100Hz）
```c
void TIMG6_IRQHandler(void) {
    Tracking_Tick();      // 读传感器 → PD 计算 → 电机输出
    Button_Tick();        // 按键消抖 + 状态机
    Task_Manager_Tick();  // 任务调度（检查完成条件）
}
```
**职责：**
- 循迹控制（最高优先级，需要快速响应）
- 按键扫描（10ms 消抖周期）
- 任务状态转换（检查编码器距离、停车标志）

**为什么是 10ms？**
- 车速 0.5m/s 时，10ms 移动 5mm（足够快的采样率）
- 红外传感器响应时间 < 1ms，10ms 采样不丢信息
- 比 5ms 更省 CPU，比 20ms 更精确

#### TIMG7 中断（20ms 周期，50Hz）
```c
void TIMG7_IRQHandler(void) {
    Vision_PID_Tick();    // 读球位置 → PID 计算 → 舵机输出
}
```
**职责：**
- 视觉 PID 控制（钢球平衡）
- 舵机 PWM 输出（标准 50Hz 刷新率）

**为什么是 50Hz？**
- K230 视觉模块输出帧率 ≈ 50fps
- 舵机标准 PWM 频率 50Hz（20ms 周期）
- 钢球动力学频率 < 10Hz，50Hz 采样远超奈奎斯特频率

#### UART0 中断（异步触发）
```c
void UART0_IRQHandler(void) {
    uint8_t ch = DL_UART_receiveData(UART0);
    K230_Parse_Byte(ch);  // 解析 "dx=±123\n" 协议
}
```
**职责：**
- 接收 K230 视觉数据（非阻塞）
- 协议解析（状态机解析，容错性强）

**为什么异步？**
- 视觉数据到达时间不确定（取决于 K230 处理速度）
- 中断触发立即接收，不丢帧
- 解析逻辑轻量（< 50μs），不阻塞其他中断

**中断优先级配置：**
```
UART0（优先级 1，最高）  → 视觉数据实时性要求高
TIMG6（优先级 2，中）    → 循迹控制不能被长时间抢占
TIMG7（优先级 3，低）    → 舵机允许轻微抖动
```

**并发安全机制：**
```c
// 所有跨中断共享变量都声明为 volatile
volatile uint8_t tracking_enabled = 0;      // 主循环写，TIMG6 读
volatile int32_t track_error = 0;           // TIMG6 写，主循环读
volatile int32_t ball_position = 0;         // UART0 写，TIMG7 读

// 关键区域用中断屏蔽保护
__disable_irq();
task_state = TASK_RUNNING;
__enable_irq();
```

**双表驱动设计：**
1. **页面表**（`pages.c`）：`s_page_table[5]` 存储每页的 `on_key` / `on_run` / `on_leave` 函数指针，新增页面只需表里加 1 行
2. **任务配置表**（`task_manager.c`）：`s_task_cfg[7]` 封装每任务的循迹使能、稳球使能、车速、目标序列

**时序协同示例（单周期 40ms）：**
```
0ms:   TIMG6 触发 → 循迹 PD 计算 → 左 PWM=15, 右 PWM=18
3ms:   UART0 接收 → "dx=+25\n" → ball_position = +25
10ms:  TIMG6 触发 → 循迹控制（track_error = +2）
20ms:  TIMG7 触发 → 视觉 PID 计算 → servo_output = 1520μs
30ms:  TIMG6 触发 → 循迹控制
40ms:  TIMG7 触发 → 稳球控制
```

**循环与视觉的解耦：**
- `tracking_enabled` 标志独立控制循迹启停
- `vision_enabled` 标志独立控制稳球启停
- 任务管理器查表决定每个任务启用哪些功能

![系统架构思维导图](思维导图.png)

## 📁 项目结构

```
chezaipinghenggunqiuyundongkongzhixitong/
├── main.c                      # 主程序入口（60 行）
├── sys/
│   ├── system.c/h              # 系统初始化、中断配置
│   └── clock.c/h               # 时钟配置
├── driver/
│   ├── motor.c/h               # 电机驱动（PWM）
│   ├── infrared.c/h            # 红外传感器（ADC）
│   ├── servo.c/h               # 舵机控制（PWM）
│   ├── oled.c/h                # OLED 显示（I2C）
│   └── uart_k230.c/h           # K230 视觉串口
├── app/
│   ├── track.c/h               # 循迹控制（PD 控制器）
│   ├── vision_pid.c/h          # 视觉 PID（稳球控制）
│   ├── pages.c/h               # 页面表驱动
│   ├── task_manager.c/h        # 任务配置表
│   └── menu.c/h                # 菜单逻辑
└── SDK/                        # MSPM0 SDK 库文件
```

## 🚀 使用说明

### 开发环境配置

1. 安装 **Code Composer Studio (CCS) 12.6+**（TI 官方 IDE）
2. 下载 **MSPM0G3507 SDK v2.01+**（[TI 官网](https://www.ti.com.cn/tool/cn/MSPM0-SDK)）
3. 连接 **XDS110 调试器**到天猛星开发板

### 编译与下载

1. 在 CCS 中打开本工程
2. 配置 SDK 路径（Project Properties → Resource → Linked Resources）
3. 点击「Build Project」编译
4. 点击「Debug」下载并运行

### 参数调节

核心参数在 `app/config.h` 中配置：

- **循迹 PD 参数**：`TRACK_KP`、`TRACK_KD`
- **视觉 PID 参数**：`VISION_KP`、`VISION_KI`、`VISION_KD`
- **车速设定**：`SPEED_TASK_1` ~ `SPEED_TASK_6`
- **停车精度**：`STOP_THRESHOLD`（编码器脉冲数）

#### 串口调参与实时观测

**硬件连接：**
- MSPM0G3507 UART1（调试口）→ USB-TTL → 电脑
- 波特率：115200（在 `uart.c` 中配置）

**Vofa+ 实时波形观测：**
```c
// 在 tracking.c 或 vision_pid.c 中添加
printf("%d,%d,%d\n", track_error, left_pwm, right_pwm);    // 循迹调试
printf("%d,%d\n", ball_position, servo_output);             // 视觉调试
```

**循迹 PD 调参步骤：**
1. **测试直行稳定性**：
   - 在直线赛道上启动，观察车身是否左右摆动
   - 摆动明显 → Kp 过大，减小到 1.2
   - 跟线慢、容易跑偏 → Kp 过小，增大到 1.8
   
2. **测试转弯响应**：
   - 在弯道上测试，观察转弯是否平滑
   - 转弯后振荡 → Kd 不足，增大到 0.5
   - 转弯迟钝 → Kd 过大，减小到 0.2

**视觉 PID 调参步骤：**
1. **只开 P（Ki=0, Kd=0）**：
   - 手动移动钢球，观察回中速度
   - 回中太慢 → Kp 偏小，增大到 0.15
   - 振荡不止 → Kp 过大，减小到 0.08
   
2. **加 I 消除稳态误差**：
   - 球静止时偏离中心 ±10px → Ki 偏小，增大到 0.005
   - 球回中后超调严重 → Ki 过大，减小到 0.001
   
3. **加 D 抑制振荡**：
   - 快速推动钢球，观察系统响应
   - 超调量大 → Kd 偏小，增大到 0.5
   - 响应迟钝 → Kd 过大，减小到 0.2

**阶跃响应测试：**
```c
// 循迹测试：左右传感器轮流触发
track_error = -5;  // 模拟左偏
delay_ms(2000);
track_error = +5;  // 模拟右偏
delay_ms(2000);
// 通过 Vofa+ 观察 PWM 输出波形的跟随效果
```

**脉冲观测：**
```c
// 打印编码器脉冲和目标位置
printf("%d,%d,%d\n", left_encoder, right_encoder, target_position);
```
通过波形观察：
- 左右轮速度是否一致（检测车身对称性）
- 加速过程是否平滑（检测电机响应）
- 停车位置精度（检测编码器准确性）

### OLED 菜单操作

- **按键 1（上）**：上翻页面
- **按键 2（确认）**：选择当前项/启动任务
- **按键 3（下）**：下翻页面

**5 个页面：**
1. 主页（显示当前状态）
2. 任务选择（6 个预设任务）
3. 参数调节（PID/速度）
4. 系统信息（电池电压/版本号）
5. 调试模式（实时数据）

## 📊 任务配置示例

| 任务 | 循迹 | 稳球 | 车速 | 目标序列 |
|------|------|------|------|----------|
| 任务 1 | ✓ | ✗ | 50% | 直行 5m |
| 任务 2 | ✓ | ✓ | 40% | 直行 3m → 稳球 10s |
| 任务 3 | ✓ | ✓ | 60% | 循环赛道 + 动态稳球 |

## 🏆 竞赛成绩

- **2026 年全国大学生电子设计竞赛 H 题**：完成全部基本要求 + 发挥部分

## 🤝 参与贡献

1. **槐序**（Sherlock Holmes）  
2. **拾忆**  
3. **哦哦**（いずみ さぎり）

欢迎 Fork 本仓库并提交改进方案。

## 📝 参考资料

- TI MSPM0G3507 官方文档：[TI MSPM0 系列](https://www.ti.com.cn/microcontrollers-mcus-processors/arm-based-microcontrollers/arm-cortex-m0-mcus/mspm0-portfolio/overview.html)
- 电赛官网：[全国大学生电子设计竞赛](http://www.nuedc.com.cn/)

---

**开发者**：xiazhuo  
**团队成员**：槐序、拾忆、哦哦
