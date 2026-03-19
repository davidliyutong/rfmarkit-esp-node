# 11 - OTA 升级与分区布局

## Flash 分区表

**文件**: `manifests/partitions.csv`

| 名称 | 类型 | 子类型 | 大小 | 说明 |
|------|------|--------|------|------|
| nvs | data | nvs | 16 KB | 非易失性存储 |
| otadata | data | ota | 8 KB | OTA 元数据 |
| phy_init | data | phy | 4 KB | PHY 校准数据 |
| ota_0 | app | ota_0 | 1600 KB | 主应用分区 |
| ota_1 | app | ota_1 | 1600 KB | 备用应用分区（回滚） |
| nvs_keys | data | nvs_keys | 4 KB | NVS 加密密钥 |

### 分区布局示意

```
Flash 地址空间:
┌─────────────────┐ 0x0000
│   Bootloader    │
├─────────────────┤ 0x9000
│      NVS        │ 16 KB
├─────────────────┤ 0xD000
│    OTADATA      │ 8 KB
├─────────────────┤ 0xF000
│    PHY_INIT     │ 4 KB
├─────────────────┤
│     OTA_0       │ 1600 KB  ← 当前运行的固件
├─────────────────┤
│     OTA_1       │ 1600 KB  ← OTA 下载目标
├─────────────────┤
│   NVS_KEYS      │ 4 KB
└─────────────────┘
```

## OTA 升级流程

### 触发方式

1. **REST API**: `POST /v1/system/upgrade` 带 OTA URL
2. **设备发现**: 控制器回复中包含 OTA 服务器地址
3. **NVS 配置**: 预设 `OTA_HOST` 变量

### 升级流程

```
触发 OTA
    │
    ├── set_sys_event(OTA_TRIGGERED)
    │
    ▼
sys_ota_perform()
    │
    ├── 1. 查询当前运行分区 (esp_ota_get_running_partition)
    ├── 2. 获取下一个 OTA 分区 (esp_ota_get_next_update_partition)
    ├── 3. 配置 HTTPS OTA 参数
    │       ├── OTA URL (来自 NVS 或 API 请求)
    │       └── SSL 证书（如配置）
    ├── 4. 执行 OTA 下载 (esp_https_ota)
    │       ├── HTTP GET 下载固件
    │       ├── 写入目标分区
    │       └── 校验完整性
    ├── 5. 设置下次启动分区 (esp_ota_set_boot_partition)
    └── 6. 重启设备 (esp_restart)
```

### 回滚保护

```
重启后:
    │
    ▼
app_main() → sys_ota_guard()
    │
    ├── 检查 esp_ota_get_state() == ESP_OTA_IMG_PENDING_VERIFY ?
    │
    ├── 是 → 运行验证
    │         ├── IMU 自检通过？
    │         ├── Wi-Fi 连接成功？
    │         └── 基本功能正常？
    │               │
    │               ├── 通过 → esp_ota_mark_app_valid_cancel_rollback()
    │               │           (确认新固件，取消回滚)
    │               │
    │               └── 失败 → esp_ota_mark_app_invalid_rollback_and_reboot()
    │                           (标记无效，回滚到上一版本并重启)
    │
    └── 否 → 正常启动（已确认的固件）
```

### 双分区切换

```
第 N 次启动:
  OTADATA 指向 OTA_0 → 运行 OTA_0 中的固件

执行 OTA 升级:
  下载新固件到 OTA_1
  OTADATA 更新为指向 OTA_1

第 N+1 次启动:
  OTADATA 指向 OTA_1 → 运行 OTA_1 中的固件
  验证通过 → 确认 OTA_1

下次 OTA 升级:
  下载新固件到 OTA_0（交替使用）
```

## 配置要点

### sdkconfig 相关配置

```
# OTA 回滚支持（v1 启用）
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y

# 自定义分区表
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="manifests/partitions.csv"
```

### OTA 服务器要求

- 支持 HTTP/HTTPS GET 请求
- 返回原始二进制固件文件
- URL 通过 NVS `OTA_HOST` 配置或发现协议动态下发

## 固件版本管理

- 版本号从 Git tag 提取（如 `v3.3.2`）
- 编译时注入为 `FIRMWARE_VERSION` 宏
- REST API `/v1/system/info` 返回当前版本
- OTA 前后版本可通过 API 查询
