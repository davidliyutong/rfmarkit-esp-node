# 06 - 事件系统设计

## 概述

系统采用 FreeRTOS Event Group + ESP Event Loop 双层事件机制：

- **Event Group**: 位掩码方式的轻量级同步，用于任务间状态通知
- **ESP Event Loop**: 结构化事件分发，用于 `app_system_loop` 中的异步事件处理

## Event Group 架构

### 三个事件组

```
┌─────────────────────────────┐
│      sys_event_group        │  系统级状态通知
│  (跨任务、跨组件)             │
├─────────────────────────────┤
│      task_event_group       │  任务内部协调
│  (防止并发操作)               │
├─────────────────────────────┤
│      wifi_event_group       │  Wi-Fi 专用
│  (连接状态)                   │
└─────────────────────────────┘
```

### 系统事件位定义

```c
// Wi-Fi 相关
EV_SYS_WIFI_CONFIG_UPDATED_BIT    // Wi-Fi 配置已更新
EV_SYS_WIFI_CONNECTED_BIT         // STA 已连接（IP 已获取）
EV_SYS_WIFI_DISCONNECTED_BIT      // STA 已断开
EV_SYS_WIFI_FAIL_BIT              // 连接失败（重试耗尽）

// 服务状态
EV_SYS_NTP_SYNCED_BIT             // NTP 时间已同步
EV_SYS_DISCOVERY_COMPLETED_BIT    // 发现协议已完成

// 控制信号
EV_SYS_LED_STATUS_CHANGED_BIT     // LED 状态变更请求
EV_SYS_OTA_TRIGGERED_BIT          // OTA 升级触发
EV_SYS_MODE_CHANGE_BIT            // 操作模式切换
EV_SYS_POWER_MGMT_BIT             // 电源状态变更
```

### 事件流转示例

#### Wi-Fi 连接流程

```
sys_wifi_try_connect()
    │
    ├── 连接成功 → set_sys_event(WIFI_CONNECTED)
    │                 │
    │                 ├── app_data_client: 开始 UDP 发送
    │                 ├── system_loop: 启动发现定时器
    │                 └── system_loop: 启动 NTP 同步定时器
    │
    └── 连接失败 → set_sys_event(WIFI_FAIL)
                      │
                      └── system_loop: 触发 Wi-Fi 重连事件
```

#### 模式切换流程

```
按钮单击 / REST API
    │
    ▼
set_sys_event(MODE_CHANGE)
    │
    ▼
app_system_loop → sys_mode_change_handler()
    │
    ├── ACTIVE → STANDBY
    │     ├── 停止 IMU 采集
    │     ├── LED 设为慢呼吸
    │     └── 重置电源节能定时器
    │
    └── STANDBY → ACTIVE
          ├── 启动 IMU 采集
          ├── LED 设为序列编码
          └── 启动电源节能定时器
```

## ESP Event Loop

### 私有事件循环

`app_system_loop` 创建独立的 ESP 事件循环用于结构化事件处理：

```c
esp_event_loop_args_t loop_args = {
    .queue_size = 8,
    .task_name = "sys_evt",
    .task_priority = 5,
    .task_stack_size = 4096,
    .task_core_id = tskNO_AFFINITY,
};
esp_event_loop_create(&loop_args, &g_mcu.sys_event_loop);
```

### 事件类型与处理器

| 事件基类 | 事件 ID | 处理器 | 功能 |
|----------|---------|--------|------|
| `SYSTEM_TIME_SYNC_EVENT` | `SYSTEM_EVENT_TIME_SYNC` | `sys_time_sync_handler` | NTP 同步 |
| `SYSTEM_DISCOVERY_EVENT` | `SYSTEM_EVENT_DISCOVERY` | `sys_discovery_handler` | 设备发现 |
| `SYSTEM_LED_STATUS_EVENT` | `SYSTEM_EVENT_LED_STATUS` | `sys_led_status_handler` | LED 更新 |
| `SYSTEM_WIFI_RECONNECT_EVENT` | `SYSTEM_EVENT_WIFI_RECONNECT` | `sys_wifi_reconnect_event_handler` | Wi-Fi 恢复 |
| `SYSTEM_MODE_CHANGE_EVENT` | `SYSTEM_EVENT_MODE_CHANGE` | `sys_mode_change_handler` | 模式切换 |
| `SYSTEM_POWER_MGMT_EVENT` | `SYSTEM_EVENT_POWER_MGMT` | `sys_power_mgmt_handler` | 电源管理 |
| `SYSTEM_OTA_EVENT` | `SYSTEM_EVENT_OTA` | — | OTA 处理 |

### 事件分发流程

```
定时器到期 / 外部触发
    │
    ▼
set_sys_event(对应位)
    │
    ▼
app_system_loop 的主循环:
    xEventGroupWaitBits(sys_event_group, ALL_BITS, ...)
    │
    ├── 检测到 NTP 位 → esp_event_post_to(TIME_SYNC_EVENT)
    ├── 检测到发现位  → esp_event_post_to(DISCOVERY_EVENT)
    ├── 检测到 LED 位  → esp_event_post_to(LED_STATUS_EVENT)
    ├── 检测到 Wi-Fi 位 → esp_event_post_to(WIFI_RECONNECT_EVENT)
    ├── 检测到模式位  → esp_event_post_to(MODE_CHANGE_EVENT)
    └── 检测到电源位  → esp_event_post_to(POWER_MGMT_EVENT)
    │
    ▼
ESP Event Loop 任务分发到注册的处理器
```

## 定时器

`app_system_loop` 管理三个 ESP 定时器：

| 定时器 | 周期 | 触发事件 |
|--------|------|----------|
| `discovery_timer` | 15 秒 | `EV_SYS_DISCOVERY_COMPLETED_BIT` |
| `time_sync_timer` | 300 秒 | `EV_SYS_NTP_SYNCED_BIT` |
| `power_mgmt_timer` | 20 秒 | `EV_SYS_POWER_MGMT_BIT` |

定时器在 Wi-Fi 连接成功后启动，Wi-Fi 断开时不主动停止（依赖事件处理器中的状态检查）。

## 任务事件组（并发控制）

任务事件组用于防止同类操作并发执行：

```c
// NTP 同步防重入
if (get_task_event(EV_TASK_TIME_SYNC_BIT)) {
    return;  // 已有 NTP 同步在进行
}
set_task_event(EV_TASK_TIME_SYNC_BIT);
// ... 执行 NTP 同步 ...
clear_task_event(EV_TASK_TIME_SYNC_BIT);
```

这种模式确保即使定时器多次触发，同一操作也不会并发执行。
