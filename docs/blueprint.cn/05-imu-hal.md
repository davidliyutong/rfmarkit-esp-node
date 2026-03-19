# 05 - IMU 硬件抽象层

## 设计理念

IMU 子系统采用编译时多态设计，通过 Kconfig 在构建时选择具体驱动实现，上层代码通过统一的 `imu_interface_t` 接口访问传感器，无需关心底层硬件差异。

## 接口定义

### imu_interface_t（统一接口）

```c
typedef struct {
    imu_t *p_imu;                                          // IMU 实例指针
    esp_err_t (*init)(imu_t *, imu_config_t *);            // 初始化硬件
    esp_err_t (*read)(imu_t *, imu_dgram_t *, bool);       // 读取帧（含 CRC 选项）
    esp_err_t (*toggle)(imu_t *, bool);                    // 启用/禁用 IMU
    int (*enabled)(imu_t *);                               // 查询是否启用
    esp_err_t (*self_test)(imu_t *);                       // 硬件自检
    void (*soft_reset)(imu_t *);                           // 软件复位
    void (*hard_reset)(imu_t *);                           // 硬件复位（GPIO）
    void (*buffer_reset)(imu_t *);                         // 刷新内部缓冲区
    int64_t (*get_delay_us)(imu_t *);                      // 获取读取延迟
    size_t (*read_bytes)(imu_t *, uint8_t *, size_t);      // 原始字节读取
    esp_err_t (*write_bytes)(imu_t *, void *, size_t);     // 原始字节写入
} imu_interface_t;
```

### imu_dgram_t（数据帧格式）

```c
typedef struct {
    imu_data_t imu;            // 传感器原始数据
    int64_t dev_ts_us;         // 设备时间戳（微秒）
    int64_t tsf_ts_us;         // TSF 时间戳（同步用）
    uint32_t seq;              // 帧序列号
    int32_t buffer_delay_us;   // 环形缓冲区延迟
} imu_dgram_t;
```

### imu_data_t（传感器数据）

```c
typedef struct {
    float acc[3];              // 加速度 (m/s²)
    float gyr[3];              // 角速度 (rad/s)
    float mag[3];              // 磁场强度
    float quat[4];             // 四元数 (w, x, y, z)
    float euler[3];            // 欧拉角 (roll, pitch, yaw)
    uint32_t ts_ms;            // IMU 内部时间戳（毫秒）
} imu_data_t;
```

## IMU 多路复用

```c
typedef enum {
    IMU_MUX_IDLE,      // 空闲，不主动读取
    IMU_MUX_STREAM,    // 流模式：数据写入环形缓冲区
    IMU_MUX_DEBUG      // 调试模式：数据通过 WebSocket 推送
} imu_mux_t;
```

- `STREAM` 模式：`app_monitor` 任务轮询 IMU 并写入 ring buffer
- `DEBUG` 模式：REST WebSocket 端点直接读取 IMU 数据实时推送

## 驱动实现

### HI229 驱动（Hardware v1）

**通信接口**: UART 串口

| 参数 | 值 |
|------|----|
| 波特率 | 115200（可通过 NVS 配置） |
| 数据位 | 8N1 |
| 帧长度 | 82 字节 |
| 校验 | CRC |

**引脚分配（ESP32）**:

| GPIO | 功能 |
|------|------|
| GPIO 17 | UART RX |
| GPIO 16 | UART TX |
| GPIO 4 | IMU 使能（RTC GPIO） |
| GPIO 18 | 同步输入 |
| GPIO 19 | 同步输出 |
| GPIO 15 | IMU 复位 |

**数据输出**: 四元数、欧拉角、原始传感器数据

**特性**:
- UART 串口协议，帧格式固定
- 支持通过 NVS 动态修改波特率
- 硬件复位通过 GPIO 控制

### BNO08X 驱动（Hardware v2）

**通信接口**: SPI

| 参数 | 值 |
|------|----|
| SPI 时钟 | 3 MHz |
| SPI 模式 | ESP-IDF SPI Master |
| SPI 主机 | VSPI_HOST (ESP32) / SPI2_HOST (ESP32-S3) |

**引脚分配（ESP32-S3）**:

| GPIO | 功能 |
|------|------|
| GPIO 13 | SPI MOSI |
| GPIO 11 | SPI MISO |
| GPIO 12 | SPI SCLK |
| GPIO 10 | SPI CS |
| GPIO 9 | 中断（RTC GPIO） |
| GPIO 8 | 复位（RTC GPIO） |
| GPIO 3 | 唤醒（RTC GPIO） |

**数据输出**: 四元数和原始传感器数据

**特性**:
- SPI 高速通信
- 中断驱动的数据就绪通知
- 支持硬件复位和唤醒控制
- RTC GPIO 支持深度睡眠唤醒

## 编译时选择机制

```
Kconfig 选择
    │
    ├── CONFIG_IMU_SENSOR_HI229
    │     └── CMake: REQUIRES sys hi229
    │           └── 链接 lib/hi229 驱动
    │
    └── CONFIG_IMU_SENSOR_BNO08X
          └── CMake: REQUIRES sys bno08x
                └── 链接 lib/bno08x 驱动
```

`components/imu/imu.c` 中的 `imu_interface_init()` 在运行时根据编译配置初始化对应驱动，并填充 `imu_interface_t` 函数指针表。

## 数据流

```
IMU 硬件
    │
    ├── HI229: UART 接收 → 帧解析 → CRC 校验
    │
    └── BNO08X: SPI 读取 → SHTP 协议解析
    │
    ▼
imu_dgram_t (统一格式)
    │
    ├── dev_ts_us: 设备时间戳
    ├── seq: 递增序列号
    └── imu_data_t: 传感器数据
    │
    ▼
ring_buf_push() → 环形缓冲区 → ring_buf_peek() → 网络发送
```

## 帧率控制

当启用 `CONFIG_EN_READ_FPS_LIM` 时，`app_monitor` 使用 `esp_timer` 精确控制采样间隔：

```
目标间隔 = 1,000,000 / TARGET_FPS (微秒)

每次读取后：
  实际间隔 = 当前时间 - 上次读取时间
  if (实际间隔 < 目标间隔):
      等待差值
```

`TARGET_FPS` 可通过 NVS 运行时修改，范围 1-400 Hz。
