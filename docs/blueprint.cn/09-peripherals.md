# 09 - 外设驱动

## LED 控制（blink 组件）

### 硬件配置

| 参数 | 值 |
|------|----|
| GPIO | GPIO_NUM_7 |
| 驱动 | LEDC PWM |
| 定时器 | Timer 0 |
| 通道 | Channel 0 |
| PWM 频率 | 4 kHz |
| 最大占空比 | 4000（10-bit） |
| 更新间隔 | 50ms（定时器中断） |

### LED 模式

```c
typedef enum {
    LED_OFF,            // 关闭
    LED_ON,             // 常亮
    LED_DUTY,           // 亮度调节 (0-100%)
    LED_FAST_FLASH,     // 快速闪烁 (50ms 开/关)
    LED_FAST_BREATH,    // 快速呼吸灯 (16 级渐变, 50ms/级)
    LED_SLOW_BREATH,    // 慢呼吸灯 (16 级稀疏渐变)
    LED_SEQ_ENC,        // Hamming 编码序列
} led_mode_t;
```

### Hamming 编码标识 (`LED_SEQ_ENC`)

用于动作捕捉场景中标识各标记点的唯一 ID：

- 将设备 ID 编码为 Hamming(24) 码
- 通过 LED 闪烁序列表示二进制位
- 上位机摄像头可通过识别闪烁模式关联设备

### API

```c
void blink_msp_init(void);              // 初始化 LEDC 硬件
void blink_led_on(void);                // LED 常亮
void blink_led_off(void);               // LED 关闭
void blink_led_set_duty(uint8_t pct);   // 设置亮度百分比
void blink_led_fast_flash(void);        // 快速闪烁
void blink_led_fast_breath(void);       // 快速呼吸
void blink_start_seq_enc_pattern(void); // 启动序列编码
```

### LED 状态与系统模式对应

| 系统状态 | LED 模式 | 说明 |
|----------|----------|------|
| 启动中 | 快速闪烁 | 初始化进行中 |
| 待机 | 慢呼吸 | 等待激活 |
| 活跃 | 序列编码 | 数据流传输中 |
| 省电 | 关闭 | 最小功耗 |
| 深度睡眠 | 关闭 | 完全断电 |
| 错误 | 快速闪烁 | 异常状态 |

## 按钮（button 组件）

### 硬件配置

| 参数 | 值 |
|------|----|
| GPIO | GPIO_NUM_0 |
| 激活电平 | 低电平有效 |
| 上拉 | 内部上拉启用 |
| 中断类型 | 边沿触发 |

### 状态机

```
IDLE ──(下降沿)──→ PRESSED ──(上升沿)──→ RELEASED ──(事件处理)──→ IDLE
                      │
                      └── 记录按下时间
                              │
                              ├── 按下时长 < 1000ms → CLICK 事件
                              └── 按下时长 ≥ 1000ms → LONG_PRESS 事件
```

### 事件处理

| 事件 | 处理 |
|------|------|
| CLICK | 切换操作模式 (STANDBY ↔ ACTIVE) |
| LONG_PRESS | 进入深度睡眠 |

**实现**:
- GPIO 中断将事件推入 FreeRTOS 队列 `button_event_queue`
- 按钮守护任务从队列读取事件并执行对应操作
- 防抖通过时间判断实现

## 电池监测（battery 库）

### 硬件配置

| 目标 | 使能 GPIO | ADC 通道 |
|------|----------|---------|
| ESP32 | GPIO 25 | ADC6 |
| ESP32-S3 | GPIO 2 | ADC0 |

### API

```c
void battery_msp_init(void);          // ADC 通道初始化
uint8_t battery_read_level(void);     // 读取电池电量百分比
```

### 工作流程

1. 拉高使能 GPIO 激活电池分压电路
2. ADC 采样电池电压
3. 电压值转换为百分比
4. 拉低使能 GPIO 关闭分压（省电）

## 引脚总分配

### ESP32（Hardware v1）

| GPIO | 功能 | 方向 | 备注 |
|------|------|------|------|
| 0 | 按钮 | 输入 | 低电平有效，内部上拉 |
| 4 | IMU 使能 | 输出 | RTC GPIO |
| 7 | LED | 输出 | LEDC PWM |
| 15 | IMU 复位 | 输出 | |
| 16 | IMU UART TX | 输出 | |
| 17 | IMU UART RX | 输入 | |
| 18 | IMU 同步输入 | 输入 | |
| 19 | IMU 同步输出 | 输出 | |
| 25 | 电池使能 | 输出 | ADC6 |

### ESP32-S3（Hardware v2）

| GPIO | 功能 | 方向 | 备注 |
|------|------|------|------|
| 0 | 按钮 | 输入 | 低电平有效，内部上拉 |
| 2 | 电池使能 | 输出 | ADC0 |
| 3 | IMU 唤醒 | 输出 | RTC GPIO |
| 7 | LED | 输出 | LEDC PWM |
| 8 | IMU 复位 | 输出 | RTC GPIO |
| 9 | IMU 中断 | 输入 | RTC GPIO |
| 10 | SPI CS | 输出 | |
| 11 | SPI MISO | 输入 | |
| 12 | SPI SCLK | 输出 | |
| 13 | SPI MOSI | 输出 | |
