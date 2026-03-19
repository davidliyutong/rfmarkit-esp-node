# 12 - CI/CD 流水线

## GitHub Actions 工作流

**文件**: `.github/workflows/build-firmware.yml`

### 触发条件

| 触发器 | 条件 |
|--------|------|
| `workflow_dispatch` | 手动触发 |
| `release` | 发布 Release 时（published 事件） |

### 构建矩阵

两种硬件变体并行构建：

| 变体 | 芯片目标 | sdkconfig 文件 |
|------|---------|---------------|
| v1 | esp32 | `sdkconfig.defaults;sdkconfig.defaults.v1` |
| v2 | esp32s3 | `sdkconfig.defaults;sdkconfig.defaults.v2` |

### 构建环境

- **Docker 镜像**: `espressif/esp-idf:v5.5.3`
- **构建命令**:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v1" \
       set-target esp32 build

idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v2" \
       set-target esp32s3 build
```

### 产物

| 文件 | 内容 | 用途 |
|------|------|------|
| `rfmarkit-esp-node-v1.bin` | ESP32 固件二进制 | OTA / 烧录 |
| `rfmarkit-esp-node-v2.bin` | ESP32-S3 固件二进制 | OTA / 烧录 |
| `rfmarkit-esp-node-v1.tar.gz` | 二进制 + ELF | 调试分析 |
| `rfmarkit-esp-node-v2.tar.gz` | 二进制 + ELF | 调试分析 |

### 流水线步骤

```
workflow_dispatch / release publish
    │
    ├─── Job: build-v1 ─────────────────────┐
    │    │                                   │
    │    ├── Checkout 代码                    │
    │    ├── idf.py set-target esp32         │
    │    ├── idf.py build (sdkconfig v1)     │
    │    ├── 上传 .bin 产物                   │
    │    └── 上传 .tar.gz 产物               │
    │                                        │
    ├─── Job: build-v2 ─────────────────────┐│
    │    │                                   ││
    │    ├── Checkout 代码                    ││
    │    ├── idf.py set-target esp32s3       ││
    │    ├── idf.py build (sdkconfig v2)     ││
    │    ├── 上传 .bin 产物                   ││
    │    └── 上传 .tar.gz 产物               ││
    │                                        ││
    └─── Job: release (仅 release 触发) ─────┘│
         │                                    │
         ├── 等待两个构建完成                   │
         ├── 下载所有产物                      │
         └── 创建 Draft Pre-release            │
              ├── rfmarkit-esp-node-v1.bin     │
              └── rfmarkit-esp-node-v2.bin     │
```

### Release 发布

- CI 自动创建 **Draft Pre-release**
- 需要手动确认发布（防止误发布）
- Release 附件包含两个硬件变体的固件文件
- 版本号从 Git tag 自动提取

## 本地构建

### 环境搭建

```bash
make setup               # 一次性：克隆 ESP-IDF + 安装工具链
```

### 日常开发

```bash
make set_hardware_v1      # 或 make set_hardware_v2
make build                # 编译
make flash-monitor        # 烧录并打开串口监视器
```

### 完全重建

```bash
make fullclean            # 清除所有构建产物和 sdkconfig
make set_hardware_v1      # 重新配置目标
make build                # 完全重新编译
```

## 版本号与 Git 标签

1. 开发者创建 Git tag（如 `v3.4.0`）
2. 推送 tag 到 GitHub
3. 创建 Release（触发 CI）
4. CI 构建时自动从 tag 提取版本号
5. 版本号编译进固件二进制

```
git tag v3.4.0
git push origin v3.4.0
# GitHub 上创建 Release → 触发 CI → 自动构建并附加产物
```
