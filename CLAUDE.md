# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RFMarkIt ESP Node — active marker firmware for motion capture and 6DOF tracking, built on **ESP-IDF v5.5.3**. Two hardware variants:

- **Hardware v1**: ESP32 + HI229 IMU (UART)
- **Hardware v2**: ESP32-S3 + BNO08X IMU (SPI)

## Build Commands

```bash
make setup               # Bootstrap local ESP-IDF (one-time)
make update              # Update local ESP-IDF to IDF_VERSION
make set_hardware_v1     # Configure for ESP32 + HI229
make set_hardware_v2     # Configure for ESP32-S3 + BNO08X
make build               # Build firmware (idf.py build)
make flash               # Flash to device
make monitor             # Serial monitor
make flash-monitor       # Flash then monitor
make menuconfig          # ESP-IDF menuconfig
make clean               # Clean build
make fullclean           # Full clean including sdkconfig
```

ESP-IDF is installed locally into `.esp-idf/` with tools in `.espressif/` — all make targets auto-activate the environment, no system-wide install needed. `IDF_VERSION` (default `v5.5.3`), `IDF_PATH`, and `IDF_TOOLS_PATH` are overridable. The `.envrc` also sets these for direnv users. Firmware version is extracted from Git tags (default: 3.3.2).

## Architecture

### Entry & Task Model

`main/app_main.c` initializes hardware, creates a ring buffer for IMU data, runs IMU self-test, then launches FreeRTOS tasks:
- `app_monitor()` — IMU data collection and streaming
- `app_network()` — UDP data transmission (broadcast on port 18891)
- `app_system_loop()` — main event processing loop

### Global State

`components/sys/sys.h` defines `mcu_t`, the central system struct holding device identity, IMU ring buffer, task/timer handles, event state, and NVS-backed settings (Wi-Fi credentials, host IPs, OTA URL, NTP host). A single global instance is used throughout.

### IMU Hardware Abstraction

`components/imu/imu.h` defines a common interface (`imu_init`, `imu_read`, `imu_toggle`, `imu_self_test`, `imu_reset`). Compile-time selection via Kconfig (`CONFIG_IMU_SENSOR_BNO08X` / `CONFIG_IMU_SENSOR_HI229`) switches the linked driver. The `imu` component's CMakeLists.txt conditionally requires either `bno08x` or `hi229`.

### Event System

FreeRTOS event groups coordinate system-wide state: Wi-Fi connection, NTP sync, device discovery, LED status, OTA triggers, mode changes, and power management. Events are defined as bit flags in `sys.h`.

### Power Management

`components/sys/power_mgmt.c` implements a state machine (NORMAL_BOOT → STANDBY → ACTIVE → POWER_SAVE → DEEP_SLEEP) with peripheral tracking (Wi-Fi, BLE, IMU, LED, button, controller) and configurable power modes (PERFORMANCE, NORMAL, LOW_ENERGY).

### Network Services

- **REST API**: Port 18888, WebSocket support for debug — `components/rest_controller/`
- **Data streaming**: UDP broadcast on port 18891 — `components/apps/network.c`
- **Discovery**: Port 18889 — `components/sys/discovery.c`
- **BLE**: NimBLE stack for configuration — `components/ble/`

### Hardware Pin Definitions

`include/modelspec.h` — GPIO assignments per hardware variant, guarded by `#ifdef CONFIG_IDF_TARGET_*`.

### Configuration Constants

`include/settings.h` — network ports, default hosts, timeouts, buffer sizes, and other compile-time constants.

## SDK Configuration

- `sdkconfig.defaults` — shared settings (custom partition table, NimBLE, FreeRTOS 1000Hz tick, power management)
- `sdkconfig.defaults.v1` — v1-specific (HI229 IMU, 240MHz, QIO flash, OTA rollback)
- `sdkconfig.defaults.v2` — v2-specific (BNO08X IMU, USB-JTAG console, IRAM optimizations)

## CI/CD

GitHub Actions (`.github/workflows/build-firmware.yml`) builds both hardware variants in parallel using ESP-IDF v5.5.3 Docker image. Triggers on manual dispatch and release publish. Produces `rfmarkit-esp-node-v1.bin` and `rfmarkit-esp-node-v2.bin` artifacts.

## Flash Partition Layout

Dual OTA partitions (1600KB each) with rollback support, defined in `manifests/partitions.csv`.
