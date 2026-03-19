# 13 - 硬件变体与引脚规格

## 变体对比

| 特性 | Hardware v1 | Hardware v2 |
|------|-------------|-------------|
| **MCU** | ESP32 | ESP32-S3 |
| **IMU** | HI229 | BNO08X |
| **IMU 接口** | UART (115200 baud) | SPI (3 MHz) |
| **CPU 频率** | 240 MHz | 默认 (160 MHz) |
| **Flash 模式** | QIO | DIO |
| **调试接口** | UART 控制台 | USB-JTAG + UART |
| **BLE** | NimBLE | NimBLE (连接重试) |
| **SoftAP** | 支持（预留） | 不支持 |
| **OTA 回滚** | 启用 | 启用 |
| **IRAM 优化** | 禁用（空间受限） | LWIP IRAM 优化 |

## 引脚定义（modelspec.h）

### 通用引脚（两个变体共享）

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO_NUM_0 | 按钮 | 低电平有效，内部上拉 |
| GPIO_NUM_7 | LED | LEDC PWM 输出 |

### ESP32 + HI229（v1）

```
┌─────────────────────────────────────┐
│              ESP32                   │
│                                     │
│  GPIO 0  ──── 按钮 (低有效)          │
│  GPIO 4  ──── HI229 使能 (RTC)      │
│  GPIO 7  ──── LED (PWM)             │
│  GPIO 15 ──── HI229 复位            │
│  GPIO 16 ──── HI229 UART TX        │
│  GPIO 17 ──── HI229 UART RX        │
│  GPIO 18 ──── HI229 同步输入        │
│  GPIO 19 ──── HI229 同步输出        │
│  GPIO 25 ──── 电池使能 (ADC6)       │
│                                     │
└─────────────────────────────────────┘
```

### ESP32-S3 + BNO08X（v2）

```
┌─────────────────────────────────────┐
│            ESP32-S3                  │
│                                     │
│  GPIO 0  ──── 按钮 (低有效)          │
│  GPIO 2  ──── 电池使能 (ADC0)       │
│  GPIO 3  ──── BNO08X 唤醒 (RTC)    │
│  GPIO 7  ──── LED (PWM)             │
│  GPIO 8  ──── BNO08X 复位 (RTC)    │
│  GPIO 9  ──── BNO08X 中断 (RTC)    │
│  GPIO 10 ──── SPI CS               │
│  GPIO 11 ──── SPI MISO             │
│  GPIO 12 ──── SPI SCLK             │
│  GPIO 13 ──── SPI MOSI             │
│                                     │
└─────────────────────────────────────┘
```

## 编译时变体选择

### Kconfig 选择

```
main/Kconfig.projbuild:
  choice IMU_SENSOR
    config IMU_SENSOR_HI229    → Hardware v1
    config IMU_SENSOR_BNO08X   → Hardware v2
  endchoice
```

### sdkconfig 合并

```bash
# v1 构建
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v1" build

# v2 构建
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v2" build
```

sdkconfig 文件按顺序合并，后者覆盖前者。

### CMake 条件编译

```cmake
# components/imu/CMakeLists.txt
if(CONFIG_IMU_SENSOR_BNO08X)
    idf_component_register(SRCS "imu.c" REQUIRES sys bno08x)
elseif(CONFIG_IMU_SENSOR_HI229)
    idf_component_register(SRCS "imu.c" REQUIRES sys hi229)
endif()
```

### C 代码条件编译

```c
// include/modelspec.h
#ifdef CONFIG_IDF_TARGET_ESP32
    // v1 引脚定义
    #define PIN_IMU_RX    GPIO_NUM_17
    #define PIN_IMU_TX    GPIO_NUM_16
    ...
#elif CONFIG_IDF_TARGET_ESP32S3
    // v2 引脚定义
    #define PIN_SPI_MOSI  GPIO_NUM_13
    #define PIN_SPI_MISO  GPIO_NUM_11
    ...
#endif
```

## Makefile 硬件切换

```bash
make set_hardware_v1
# 等效于:
#   idf.py set-target esp32
#   复制 sdkconfig.defaults + sdkconfig.defaults.v1

make set_hardware_v2
# 等效于:
#   idf.py set-target esp32s3
#   复制 sdkconfig.defaults + sdkconfig.defaults.v2
```

切换硬件目标会触发完全重新编译，因为目标芯片和 sdkconfig 均发生变化。

## 深度睡眠与 RTC GPIO

### v1（ESP32）

| RTC GPIO | 功能 | 深度睡眠用途 |
|----------|------|-------------|
| GPIO 4 | IMU 使能 | 睡眠期间拉低关闭 IMU |

### v2（ESP32-S3）

| RTC GPIO | 功能 | 深度睡眠用途 |
|----------|------|-------------|
| GPIO 3 | IMU 唤醒 | 可用于唤醒 BNO08X |
| GPIO 8 | IMU 复位 | 唤醒后复位 IMU |
| GPIO 9 | IMU 中断 | 可作为深度睡眠唤醒源 |

ESP32-S3 的 BNO08X 驱动利用 RTC GPIO 实现从深度睡眠状态唤醒 IMU 的能力。
