# 03 - 启动流程与任务模型

## 启动时序

`main/app_main.c` 中的 `app_main()` 是固件入口点，执行以下初始化流程：

```
app_main()
  │
  ├── init()
  │     ├── sys_init_chip()          // 打印芯片信息（型号、核心数、Flash 大小）
  │     ├── sys_init_nvs()           // 初始化 NVS，加载持久化变量
  │     ├── sys_init_events()        // 创建 FreeRTOS 事件组
  │     ├── sys_wifi_netif_init()    // 初始化 Wi-Fi 网络接口
  │     ├── power_mgmt_init()        // 初始化电源管理状态机
  │     ├── ring_buf_init()          // 创建 IMU 数据环形缓冲区
  │     └── g_imu.self_test()        // IMU 硬件自检
  │
  ├── sys_start_tasks()              // 启动 3 个 FreeRTOS 任务
  │     ├── app_monitor()            // IMU 数据采集任务
  │     ├── app_data_client()        // UDP 网络发送任务
  │     └── app_system_loop()        // 系统事件循环任务
  │
  └── sys_ota_guard()                // OTA 升级校验与回滚保护
```

## 初始化细节

### 芯片信息 (`sys_init_chip`)

读取并打印 ESP 芯片型号、核心数、Flash 大小等信息，用于调试日志。

### NVS 初始化 (`sys_init_nvs`)

1. 调用 `nvs_flash_init()` 初始化 NVS 分区
2. 若检测到 NVS 分区损坏，自动擦除并重新初始化
3. 遍历 `g_mcu_vars[]` 数组，从 NVS 加载所有持久化变量到 `g_mcu` 全局状态
4. 设置默认值（如 Wi-Fi SSID、NTP 服务器等）

### 事件组创建 (`sys_init_events`)

创建三个 FreeRTOS 事件组：

| 事件组 | 用途 |
|--------|------|
| `g_mcu.sys_event_group` | 系统级事件（Wi-Fi、NTP、发现、OTA 等） |
| `g_mcu.task_event_group` | 任务间协调（防止并发操作） |
| `g_mcu.wifi_event_group` | Wi-Fi 状态事件 |

### Wi-Fi 网络接口 (`sys_wifi_netif_init`)

初始化 ESP-NETIF 和 Wi-Fi 默认 STA 模式配置，注册 Wi-Fi 和 IP 事件处理回调。

### 电源管理 (`power_mgmt_init`)

根据启动原因（冷启动 / 深度睡眠唤醒）初始化电源状态机：
- 冷启动：`POWER_UNKNOWN → POWER_NORMAL_BOOT`
- 深度睡眠唤醒：`POWER_WAKEN`，检查是否需要重新进入睡眠

### 环形缓冲区

分配 `CONFIG_DATA_BUF_LEN`（默认 128）帧容量的环形缓冲区，用于 IMU 采集任务和网络发送任务之间的数据传递。

### IMU 自检

调用 `g_imu.self_test()` 验证 IMU 硬件通信正常。若自检失败，系统仍继续运行但不采集数据。

## FreeRTOS 任务架构

```
┌───────────────────┐    ┌───────────────────┐    ┌───────────────────┐
│   app_monitor     │    │  app_data_client  │    │  app_system_loop  │
│                   │    │                   │    │                   │
│ • 轮询 IMU 传感器  │    │ • 从环形缓冲区读取  │    │ • 事件循环         │
│ • CRC 校验        │───→│ • 打包为网络格式    │    │ • 定时器管理       │
│ • 写入环形缓冲区   │环形 │ • UDP 广播发送     │    │ • Wi-Fi/NTP/发现  │
│ • FPS 统计        │缓冲 │ • 重试机制         │    │ • 电源管理         │
│                   │区   │                   │    │ • 模式切换         │
└───────────────────┘    └───────────────────┘    └───────────────────┘
```

### 任务 1: app_monitor（IMU 数据采集）

**文件**: `components/apps/monitor.c`

**职责**:
- 以目标帧率持续轮询 IMU 传感器
- 可选的帧率限制（`CONFIG_EN_READ_FPS_LIM`）
- CRC 校验确保数据完整性
- 将验证后的 `imu_dgram_t` 帧推入环形缓冲区
- 记录 FPS 统计和丢帧计数

**前置条件**: IMU 已启用且处于 `IMU_MUX_STREAM` 模式

**运行模式**: 无阻塞，持续轮询

### 任务 2: app_data_client（UDP 网络发送）

**文件**: `components/apps/network.c`

**职责**:
- 从环形缓冲区读取 IMU 数据帧
- 转换为 `marker_packet_t` 网络包格式
- 通过 UDP 广播到端口 18891
- 填充时间戳（设备时间戳、TSF 时间戳）和处理延迟
- 错误重试（最多 3 次）

**前置条件**: Wi-Fi 已连接

### 任务 3: app_system_loop（系统事件循环）

**文件**: `components/apps/system_loop.c`

**职责**:
- 创建私有 ESP 事件循环
- 注册并调度系统事件处理器
- 管理定时器：发现（15s）、NTP 同步（300s）、电源管理（20s）
- 等待系统事件组的位掩码，分发到对应处理器

**事件处理器**:

| 事件 | 处理器 | 功能 |
|------|--------|------|
| `SYSTEM_TIME_SYNC_EVENT` | `sys_time_sync_handler` | NTP 时间同步 |
| `SYSTEM_DISCOVERY_EVENT` | `sys_discovery_handler` | 设备发现广播 |
| `SYSTEM_LED_STATUS_EVENT` | `sys_led_status_handler` | LED 状态更新 |
| `SYSTEM_WIFI_RECONNECT_EVENT` | `sys_wifi_reconnect_event_handler` | Wi-Fi 重连 |
| `SYSTEM_MODE_CHANGE_EVENT` | `sys_mode_change_handler` | 操作模式切换 |
| `SYSTEM_POWER_MGMT_EVENT` | `sys_power_mgmt_handler` | 电源状态转换 |
| `SYSTEM_OTA_EVENT` | `sys_power_mgmt_handler` | OTA 升级处理 |

## OTA 保护 (`sys_ota_guard`)

启动完成后检查 OTA 状态：
1. 查询当前启动分区
2. 若存在未确认的 OTA 更新（pending validation），执行校验
3. 校验通过则调用 `esp_ota_mark_app_valid_cancel_rollback()` 确认
4. 校验失败则自动回滚到上一分区

## FreeRTOS 配置要点

| 参数 | 值 | 说明 |
|------|----|------|
| Tick Rate | 1000 Hz | 1ms 时间分辨率 |
| 调度器 | 抢占式 | 高优先级任务可抢占低优先级 |
| 核心亲和性 | 未绑定 | 任务可在任意核心运行 |

## 工具宏

```c
// 任务创建
launch_task(fn, "name", stack_size, arg, priority, &handle)
launch_task_multicore(fn, "name", stack_size, arg, priority, &handle, core)

// 延时
os_delay_ms(ms)

// 时间获取
get_time_sec(var)    // 秒级时间戳
get_time_usec(var)   // 微秒级时间戳
```
