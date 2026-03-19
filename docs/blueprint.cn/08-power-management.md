# 08 - 电源管理

## 状态机

电源管理采用分层状态机设计，状态保存在 RTC 内存中以支持深度睡眠跨越。

### 状态定义

```c
typedef enum {
    POWER_UNKNOWN,        // 未初始化
    POWER_NORMAL_BOOT,    // 正常启动
    POWER_STANDBY,        // 待机（RF 开启，不发送数据）
    POWER_ACTIVE,         // 活跃（RF 开启，数据流传输）
    POWER_SAVE,           // 省电（RF 关闭，周期性 IMU 采样）
    POWER_DEEP_SLEEP,     // 深度睡眠（仅 RTC 运行）
    POWER_WAKEN,          // 唤醒中（从深度睡眠恢复）
} power_mgmt_state_t;
```

### 状态转换图

```
                    ┌──────────────┐
                    │ POWER_UNKNOWN│
                    └──────┬───────┘
                           │ 冷启动
                    ┌──────▼───────┐
                    │ NORMAL_BOOT  │
                    └──────┬───────┘
                           │ 初始化完成
                    ┌──────▼───────┐
            ┌──────►│   STANDBY    │◄──────────┐
            │       └──────┬───────┘           │
            │              │ 按钮/API           │
            │       ┌──────▼───────┐           │
            │       │    ACTIVE    │           │
            │       └──┬───────┬───┘           │
            │          │       │               │
            │   按钮/API│    300s 无活动         │
            │          │       │               │
            │          │  ┌────▼──────┐        │
            │          │  │ POWER_SAVE│        │
            │          │  └────┬──────┘        │
            │          │       │               │
            │          │  长按按钮 / 超时         │
            │          │       │               │
            │    ┌─────▼───────▼───┐           │
            │    │   DEEP_SLEEP    │           │
            │    └────────┬────────┘           │
            │             │ RTC 定时器 / GPIO    │
            │    ┌────────▼────────┐           │
            │    │     WAKEN       ├───────────┘
            │    └────────┬────────┘
            │             │ 需要继续睡眠
            └─────────────┘
```

## 电源模式

```c
typedef enum {
    POWER_MODE_PERFORMANCE,   // 性能模式：禁用深度睡眠，全部外设开启
    POWER_MODE_NORMAL,        // 正常模式：启用 BLE，正常运行
    POWER_MODE_LOW_ENERGY,    // 低功耗模式：禁用 BLE，启用深度睡眠
} power_mode_t;
```

| 模式 | 深度睡眠 | BLE | 适用场景 |
|------|---------|-----|---------|
| PERFORMANCE | 禁用 | 启用 | 长时间持续追踪 |
| NORMAL | 可选 | 启用 | 默认运行 |
| LOW_ENERGY | 启用 | 禁用 | 电池续航优先 |

## 外设状态追踪

```c
typedef struct {
    bool wifi_initialized;
    bool ble_enabled;
    bool imu_initialized;
    bool led_initialized;
    bool button_initialized;
    bool controller_initialized;
} power_peripheral_state_t;
```

状态机在进入/退出不同电源状态时，根据此结构体决定需要初始化或关闭哪些外设，避免重复初始化。

## RTC 内存持久化

```c
RTC_DATA_ATTR power_mgmt_ctx_t g_power_mgmt_ctx;
```

`power_mgmt_ctx_t` 使用 `RTC_DATA_ATTR` 属性存储在 RTC 内存中，在深度睡眠期间保持有效：

- 当前电源状态
- 电源模式
- 外设状态
- 唤醒原因
- 睡眠计时器配置

## 状态转换处理

### 进入待机 (`power_mgmt_on_enter_standby`)

1. 初始化 Wi-Fi（如未初始化）
2. 初始化 BLE（如模式允许且未初始化）
3. 初始化 LED（慢呼吸指示待机）
4. 初始化按钮（监听用户操作）
5. 启动 REST API 服务器
6. 尝试 Wi-Fi 连接

### 进入活跃 (`power_mgmt_on_enter_active`)

1. 初始化 IMU（如未初始化）
2. 启用 IMU 数据采集
3. LED 切换为序列编码模式（标记点标识）
4. 启动电源节能定时器

### 进入省电 (`power_mgmt_on_enter_power_save`)

1. 停止 IMU 数据采集
2. 关闭 Wi-Fi
3. 关闭 BLE（如启用）
4. LED 熄灭
5. 设置 RTC 定时器唤醒

### 进入深度睡眠 (`power_mgmt_on_enter_deep_sleep`)

1. 停止所有外设
2. 配置唤醒源：
   - RTC 定时器（可配置间隔）
   - GPIO 中断（按钮）
3. 保存状态到 RTC 内存
4. 调用 `esp_deep_sleep_start()`

### 唤醒处理 (`power_mgmt_wake_up_handler`)

1. 从 RTC 内存恢复上下文
2. 检查唤醒原因（定时器 / GPIO）
3. 决定是否需要重新进入睡眠或完全唤醒
4. 若完全唤醒，转换到 `STANDBY` 状态

## 省电定时器

- **超时阈值**: `CONFIG_POWER_SAVE_TIMEOUT_S`（默认 300 秒）
- **重置条件**: 任何有效的 IMU 数据活动
- **触发动作**: `ACTIVE → POWER_SAVE` 转换

```c
// 活动检测时重置定时器
void reset_power_save_timer(void);

// 电源管理周期性检查（每 20 秒）
power_mgmt_timer → check_timeout → 触发状态转换
```

## 按钮交互

| 操作 | 触发条件 | 响应 |
|------|----------|------|
| 单击 | 按下 < 1000ms | STANDBY ↔ ACTIVE 切换 |
| 长按 | 按下 ≥ 1000ms | 进入 DEEP_SLEEP |

按钮通过 GPIO 中断检测，事件推入 `button_event_queue`，由按钮守护任务处理。
