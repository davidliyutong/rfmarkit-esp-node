# RFMarkIt ESP Node — 设计文档

本目录包含 RFMarkIt ESP Node 固件的模块化设计文档，涵盖系统架构、各组件设计及工程实践。

## 目录

| 编号 | 文档 | 说明 |
|------|------|------|
| 01 | [项目概述](01-overview.md) | 项目简介、核心功能、系统架构总览、目录结构 |
| 02 | [构建系统与工具链](02-build-system.md) | ESP-IDF 环境、Makefile、CMake、Kconfig、sdkconfig |
| 03 | [启动流程与任务模型](03-boot-sequence.md) | app_main 初始化时序、FreeRTOS 三任务架构、工具宏 |
| 04 | [全局状态与配置管理](04-global-state.md) | mcu_t 结构、NVS 持久化变量、事件组、并发同步 |
| 05 | [IMU 硬件抽象层](05-imu-hal.md) | 统一接口设计、HI229/BNO08X 驱动、数据帧格式、帧率控制 |
| 06 | [事件系统设计](06-event-system.md) | Event Group 位掩码、ESP Event Loop、定时器、事件流转 |
| 07 | [网络服务](07-network-services.md) | UDP 数据流、REST API、设备发现协议、BLE 配网、Wi-Fi 管理 |
| 08 | [电源管理](08-power-management.md) | 状态机设计、电源模式、外设追踪、RTC 持久化、深度睡眠 |
| 09 | [外设驱动](09-peripherals.md) | LED 控制（PWM/编码）、按钮状态机、电池监测、引脚分配表 |
| 10 | [底层库](10-libraries.md) | ring_buf、libudp、libtcp、spatial 数学库 |
| 11 | [OTA 升级与分区布局](11-ota-partition.md) | Flash 分区表、双分区 OTA、回滚保护机制 |
| 12 | [CI/CD 流水线](12-ci-cd.md) | GitHub Actions、构建矩阵、产物管理、版本发布 |
| 13 | [硬件变体与引脚规格](13-hardware-variants.md) | v1/v2 对比、引脚图、编译时选择机制、RTC GPIO |
