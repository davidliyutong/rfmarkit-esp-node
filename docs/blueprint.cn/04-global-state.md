# 04 - 全局状态与配置管理

## 全局状态结构 (`mcu_t`)

`components/sys/sys.h` 定义了全局状态结构 `mcu_t`，整个系统通过单一全局实例 `g_mcu` 共享状态。

### 结构概览

```c
typedef struct {
    // 设备标识
    char device_id[13];           // MAC 地址派生的 12 字符十六进制 ID
    char ble_local_name[32];      // BLE 广播名称 (markit_XXXX)

    // IMU 数据管道
    ring_buf_t imu_ring_buf;      // IMU 数据环形缓冲区
    imu_interface_t *p_imu;       // IMU 驱动接口指针

    // FreeRTOS 同步原语
    EventGroupHandle_t sys_event_group;   // 系统事件组
    EventGroupHandle_t task_event_group;  // 任务协调事件组
    EventGroupHandle_t wifi_event_group;  // Wi-Fi 事件组

    // 任务句柄
    TaskHandle_t monitor_task;
    TaskHandle_t network_task;
    TaskHandle_t system_loop_task;

    // 定时器句柄
    esp_timer_handle_t discovery_timer;
    esp_timer_handle_t time_sync_timer;
    esp_timer_handle_t power_mgmt_timer;

    // NVS 持久化变量
    mcu_var_t vars[];             // 运行时变量表
} mcu_t;
```

## NVS 持久化变量

通过 `sys_init_nvs()` 在启动时从 NVS 加载，运行时通过 `sys_set_nvs_var()` / `sys_get_nvs_var()` 访问。

| 变量名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `WIFI_SSID` | string (64B) | — | Wi-Fi 网络名称 |
| `WIFI_PSK` | string (64B) | — | Wi-Fi 密码 |
| `DATA_HOST` | string | `255.255.255.255` | UDP 数据广播目标地址 |
| `OTA_HOST` | string (128B) | — | OTA 服务器 URL |
| `NTP_HOST` | string | — | NTP 服务器地址 |
| `TEST` | int32 | `0` | 调试标志 |
| `IMU_BAUD` | int32 | `115200` | HI229 UART 波特率 |
| `SEQ` | int32 | `0` | 帧序列号计数器 |
| `TARGET_FPS` | int32 | `100` | IMU 采样帧率 (1-400 Hz) |

### NVS 访问接口

```c
// 设置变量（自动持久化到 NVS Flash）
esp_err_t sys_set_nvs_var(const char *name, void *value);

// 获取变量值
esp_err_t sys_get_nvs_var(const char *name, void *out_value);

// REST API 访问
// GET  /v1/nvs/variable/<name>
// POST /v1/nvs/variable/<name>  body: {"value": "..."}
```

## 事件系统

### 系统事件组 (`sys_event_group`)

用于跨任务的系统级状态通知：

| 事件位 | 名称 | 触发条件 |
|--------|------|----------|
| BIT0 | `EV_SYS_WIFI_CONFIG_UPDATED_BIT` | Wi-Fi 凭据被修改 |
| BIT1 | `EV_SYS_WIFI_CONNECTED_BIT` | Wi-Fi STA 获得 IP 地址 |
| BIT2 | `EV_SYS_WIFI_DISCONNECTED_BIT` | Wi-Fi STA 断开连接 |
| BIT3 | `EV_SYS_WIFI_FAIL_BIT` | Wi-Fi 连接重试耗尽 |
| BIT4 | `EV_SYS_NTP_SYNCED_BIT` | NTP 时间同步完成 |
| BIT5 | `EV_SYS_DISCOVERY_COMPLETED_BIT` | 设备发现响应已收到 |
| BIT6 | `EV_SYS_LED_STATUS_CHANGED_BIT` | LED 状态变更请求 |
| BIT7 | `EV_SYS_OTA_TRIGGERED_BIT` | OTA 升级已触发 |
| BIT8 | `EV_SYS_MODE_CHANGE_BIT` | 操作模式变更 |
| BIT9 | `EV_SYS_POWER_MGMT_BIT` | 电源状态变更 |

### 任务事件组 (`task_event_group`)

用于防止并发操作冲突：

| 事件位 | 名称 | 用途 |
|--------|------|------|
| BIT0 | `EV_TASK_TIME_SYNC_BIT` | 防止并发 NTP 同步 |
| BIT1 | `EV_TASK_DISCOVERY_BIT` | 发现过程进行中 |
| BIT2 | `EV_TASK_LED_STATUS_BIT` | LED 状态更新排队 |
| BIT3 | `EV_TASK_WIFI_RECONNECT_BIT` | Wi-Fi 重连进行中 |

### 事件操作宏

```c
set_sys_event(EV_NAME)     // 设置事件位
clear_sys_event(EV_NAME)   // 清除事件位
get_sys_event(EV_NAME)     // 查询事件位（非阻塞）
```

## 并发与同步

### 互斥保护

| 资源 | 同步机制 | 说明 |
|------|----------|------|
| 环形缓冲区 | 内部 mutex | push/peek 操作原子化 |
| IMU 设备 | 驱动内部 mutex | 防止并发读写 |
| NVS 变量 | 逐变量信号量 | 防止并发修改同一变量 |
| 电源管理 | 全局 mutex | 状态转换原子化 |
| Wi-Fi | 事件驱动 | 无忙等待 |

### 数据流保护

IMU 数据从采集到发送的完整路径中，环形缓冲区是唯一的共享数据结构：

```
app_monitor ──push──→ [ring_buf (mutex)] ──peek──→ app_data_client
```

生产者-消费者模式通过 ring_buf 的内部 mutex 保证线程安全。

## 设备标识

设备 ID 从 MAC 地址派生，格式为 12 字符的十六进制字符串。BLE 名称为 `markit_` 前缀加设备 ID 后四位。

```
MAC: AA:BB:CC:DD:EE:FF → device_id: "aabbccddeeff"
                        → ble_local_name: "markit_eeff"
```
