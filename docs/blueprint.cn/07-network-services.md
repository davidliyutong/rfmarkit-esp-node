# 07 - 网络服务

## 概述

系统提供四类网络服务：

| 服务 | 端口 | 协议 | 用途 |
|------|------|------|------|
| IMU 数据流 | 18891 | UDP 广播 | 实时传感器数据传输 |
| REST API | 18888 | HTTP/WebSocket | 远程配置与调试 |
| 设备发现 | 18889 | UDP | 零配置组网 |
| BLE GATT | — | BLE 5.2 | 无线配网 |

## IMU 数据流（UDP 端口 18891）

### 数据包格式

`marker_packet_t` 定义了网络传输的紧凑格式：

```c
typedef struct {
    float acc[3];              // 加速度 (m/s²)
    float gyr[3];              // 角速度 (rad/s)
    float mag[3];              // 磁场强度
    float quat[4];             // 四元数 (w, x, y, z)
    uint32_t imu_ts_ms;        // IMU 内部时间戳
    int64_t dev_ts_us;         // 设备时间戳（微秒）
    int64_t tsf_ts_us;         // TSF 时间戳（同步预留）
    uint32_t seq;              // 帧序列号
    int32_t dev_delay_us;      // 处理延迟（微秒）
} marker_packet_t;
```

### 传输流程

```
ring_buf_peek()
    │
    ▼
imu_dgram_t → marker_packet_t 转换
    │
    ├── 复制传感器数据 (acc, gyr, mag, quat)
    ├── 填入设备时间戳 (dev_ts_us)
    ├── 填入 TSF 时间戳 (tsf_ts_us)
    ├── 填入序列号 (seq)
    └── 计算处理延迟 (当前时间 - 采样时间)
    │
    ▼
udp_socket_send() → 广播到 DATA_HOST:18891
```

### 配置

- **目标地址**: NVS 变量 `DATA_HOST`，默认 `255.255.255.255`（广播）
- **帧率**: 与 IMU 采样率同步（默认 100 Hz，最高 400 Hz）
- **重试**: 发送失败最多重试 3 次
- **前提**: Wi-Fi 连接已建立

## REST API（HTTP 端口 18888）

### API 版本

所有端点使用 `/v1/` 前缀，支持未来版本演进。

### 端点列表

#### 系统管理

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/v1/system/info` | 设备信息（ID、版本、时间、状态） |
| POST | `/v1/system/power` | 重启 / 关机 |
| GET | `/v1/system/upgrade` | OTA 升级状态 |
| POST | `/v1/system/upgrade` | 触发 OTA 升级 |
| POST | `/v1/system/selftest` | 执行 IMU 自检 |
| GET | `/v1/system/power_mgmt` | 电源模式查询 |
| POST | `/v1/system/power_mgmt` | 设置电源模式 |

#### NVS 变量

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/v1/nvs/variable/<name>` | 获取持久化变量值 |
| POST | `/v1/nvs/variable/<name>` | 设置持久化变量值 |

#### IMU 控制

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | `/v1/imu/calibrate` | IMU 校准 |
| POST | `/v1/imu/toggle` | 启用/禁用 IMU |
| GET | `/v1/imu/status` | IMU 状态和即时读数 |
| POST | `/v1/imu/debug/toggle` | 切换调试模式 |
| WS | `/v1/imu/debug/socket` | WebSocket 实时调试流 |

#### LED 控制

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/v1/blink/configure` | LED 设置查询 |
| POST | `/v1/blink/configure` | LED 手动模式配置 |
| GET | `/v1/blink/toggle` | LED 开关状态 |
| POST | `/v1/blink/toggle` | LED 开/关/闪烁/呼吸 |

#### 操作模式

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/v1/operation/mode` | 当前模式（active/standby） |
| POST | `/v1/operation/mode` | 切换操作模式 |

### 技术实现

- **框架**: ESP-IDF `esp_http_server`
- **数据格式**: JSON（cJSON 库）
- **WebSocket**: 原生支持，用于 IMU 调试数据实时推送
- **连接上下文**: 每连接 64 字节用户缓冲区
- **默认 404**: 返回 JSON 格式错误信息

### WebSocket 调试

当 IMU 切换到 `IMU_MUX_DEBUG` 模式时，传感器数据不再写入环形缓冲区，而是直接通过 WebSocket 推送到客户端：

```
客户端连接 /v1/imu/debug/socket
    │
    ▼
POST /v1/imu/debug/toggle → IMU_MUX_DEBUG
    │
    ▼
IMU 数据 → WebSocket 帧 → 客户端
    │
    ▼
POST /v1/imu/debug/toggle → IMU_MUX_STREAM (恢复)
```

## 设备发现协议（UDP 端口 18889）

### 请求格式

```
字节偏移:  0    1    2    3    4    5     6-17
内容:    0xe5 0xe5 0xe5 ...  Flag Reply  DeviceID (12 bytes)
         ───────────────  ──── ────  ─────────────────────
         魔数头 (5 bytes)  0x01 0x01  MAC 派生的十六进制 ID
```

### 响应格式

```c
typedef struct {
    char ntp_host[32];     // NTP 服务器地址
    char ota_host[32];     // OTA 服务器 URL
} MarkerDiscoveryReplyPacket;
```

### 发现流程

```
标记点 (Marker)                     控制器 (Controller)
     │                                     │
     │──── 广播发现请求 (每15秒) ──────────→│
     │                                     │
     │←──── 回复 NTP/OTA 配置 ─────────────│
     │                                     │
     ├── 更新 NTP_HOST                      │
     └── 更新 OTA_HOST                      │
```

**特性**:
- 周期性广播（Wi-Fi 连接后每 15 秒一次）
- 控制器动态下发 NTP 和 OTA 配置
- 相比 mDNS 更轻量，适合嵌入式场景

## BLE 配网服务

### NimBLE 配置

| 参数 | 值 |
|------|----|
| 蓝牙栈 | NimBLE 5.2 |
| 最大连接数 | 3 |
| 广播模式 | 通用可发现 + 非定向可连接 |
| 设备名称 | `markit_XXXX`（MAC 后四位） |

### GATT 服务

**Wi-Fi 配置服务** (UUID: 0x1829):

| 特征 UUID | 权限 | 用途 |
|-----------|------|------|
| 0x2B1F | 读/写 | Wi-Fi SSID 和密码配置 |

### 配网流程

```
手机 APP / BLE 工具
    │
    ├── 扫描发现 "markit_XXXX"
    ├── 连接 GATT 服务
    ├── 写入 Wi-Fi SSID + PSK
    │
    ▼
设备端:
    ├── 保存到 NVS
    ├── set_sys_event(WIFI_CONFIG_UPDATED)
    └── 触发 Wi-Fi 重连
```

## Wi-Fi 管理

### 连接策略

- **模式**: STA（Station）
- **最大重试**: 5 次（`CONFIG_WIFI_MAX_RETRY`）
- **重连**: Wi-Fi 断开后通过事件自动触发重连
- **凭据来源**: NVS 持久化（BLE 或 REST API 写入）

### 网络初始化时序

```
sys_wifi_netif_init()
    │
    ├── esp_netif_init()
    ├── esp_event_loop_create_default()
    ├── esp_netif_create_default_wifi_sta()
    ├── esp_wifi_init(DEFAULT_CONFIG)
    ├── 注册 WIFI_EVENT / IP_EVENT 处理器
    └── esp_wifi_set_mode(WIFI_MODE_STA)
```
