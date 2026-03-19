# 02 - 构建系统与工具链

## ESP-IDF 环境

项目使用本地安装的 ESP-IDF，不依赖系统全局安装：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `IDF_VERSION` | `v5.5.3` | ESP-IDF Git tag |
| `IDF_PATH` | `.esp-idf/esp-idf` | IDF 源码路径 |
| `IDF_TOOLS_PATH` | `.espressif` | 工具链缓存路径 |

`Makefile` 中所有构建目标通过 `. $(IDF_PATH)/export.sh` 自动激活环境，`direnv` 用户可通过 `.envrc` 自动加载。

## Makefile 目标

```makefile
make setup                # 克隆 ESP-IDF 并安装 esp32/esp32s3 工具链
make update               # 更新 ESP-IDF 到 IDF_VERSION
make set_hardware_v1      # 设置目标为 ESP32 + HI229
make set_hardware_v2      # 设置目标为 ESP32-S3 + BNO08X
make build                # 构建固件 (idf.py build)
make flash                # 烧录到设备
make monitor              # 串口监视器
make flash-monitor        # 烧录并监视
make menuconfig           # ESP-IDF 菜单配置
make clean                # 清理构建产物
make fullclean            # 完全清理（包括 sdkconfig）
```

### 硬件变体切换

`set_hardware_v1` 和 `set_hardware_v2` 通过以下方式切换：

1. 设置 `idf.py set-target` 为对应芯片（`esp32` / `esp32s3`）
2. 合并对应的 sdkconfig defaults 文件
3. 触发完全重新构建

## CMake 构建系统

### 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

# 注册额外组件目录
set(EXTRA_COMPONENT_DIRS
    lib/bno08x  lib/hi229  lib/battery
    lib/ring_buf  lib/libudp  lib/libtcp  lib/spatial
    components/)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rfmarkit-esp-node)

# Git tag → 固件版本号
execute_process(COMMAND git describe --tags --always ...)
add_compile_definitions(FIRMWARE_VERSION="${GIT_TAG}")
```

### 组件依赖关系

```
main
 ├── sys
 │    ├── blink (LED)
 │    ├── ble (蓝牙)
 │    ├── rest_controller (HTTP API)
 │    ├── ring_buf
 │    ├── libudp
 │    ├── spatial
 │    ├── battery
 │    └── ESP-IDF: nvs_flash, esp_wifi, bt, driver, esp_https_ota, app_update
 ├── apps
 │    ├── sys
 │    └── imu
 └── imu
      ├── sys
      └── bno08x | hi229 (条件编译)
```

### 条件编译机制

`components/imu/CMakeLists.txt` 根据 Kconfig 选择链接不同的 IMU 驱动：

```cmake
if(CONFIG_IMU_SENSOR_BNO08X)
    idf_component_register(... REQUIRES sys bno08x)
elseif(CONFIG_IMU_SENSOR_HI229)
    idf_component_register(... REQUIRES sys hi229)
endif()
```

## Kconfig 配置

`main/Kconfig.projbuild` 定义的配置项：

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `CONFIG_IMU_SENSOR_HI229` | bool | 选择 HI229 驱动 |
| `CONFIG_IMU_SENSOR_BNO08X` | bool | 选择 BNO08X 驱动 |
| `CONFIG_USE_LINEAR_ACCELERATION` | bool | 使用线性加速度（去除重力） |
| `CONFIG_USE_GYRO` | bool | 启用陀螺仪输出 |

## SDK 配置文件

### sdkconfig.defaults（共享）

- 自定义分区表 (`manifests/partitions.csv`)
- NimBLE 蓝牙栈，最多 3 个并发连接
- FreeRTOS 1000 Hz tick rate
- 编译优化等级 `-O2`
- HTTP 服务器启用 WebSocket
- Wi-Fi 大缓冲区 (RX 25 + TX 128 + 静态 128)

### sdkconfig.defaults.v1（ESP32 + HI229）

- HI229 IMU 选择
- 240 MHz CPU 频率
- QIO Flash 模式
- OTA 回滚支持
- 增大 UDP PCB 数量 (512)
- 启用 SoftAP（预留）
- 禁用 IRAM 优化（ESP32 IRAM 空间有限）

### sdkconfig.defaults.v2（ESP32-S3 + BNO08X）

- BNO08X IMU 选择
- USB-JTAG 辅助控制台
- LWIP IRAM 优化
- BLE 连接重试
- 禁用 SoftAP

## 版本号注入

CMake 在构建时执行 `git describe --tags --always`，将结果作为 `FIRMWARE_VERSION` 宏注入所有源文件。若无 Git tag，回退到默认版本 `3.3.2`。
