# 设计文档目录

本目录描述当前固件行为。旧稿在 [`docs/blueprint.cn`](../blueprint.cn)。两边冲突时，以本目录和源码为准。

## 建议阅读顺序

先看整机怎么起来，再沿一条 IMU 样本走数据面，然后看配置从哪进来，最后看灯、键、电池、电源和升级。

| 文档 | 一句话 |
| --- | --- |
| [00-overview.md](00-overview.md) | 变体、任务、控制面与数据面，以及样本和配置怎么交接。 |
| [hardware.md](hardware.md) | 编译期选定的 SoC、IMU 和 GPIO，没有运行时板级探测。 |
| [boot-and-tasks.md](boot-and-tasks.md) | `app_main` 的顺序，以及三条业务任务开始干活的门闩。 |
| [events.md](events.md) | 三组事件位如何译成私有循环上的处理函数。 |
| [global-state.md](global-state.md) | `g_mcu` 里哪些字段是电平、哪些只是启动时从 NVS 抄来的缓存。 |
| [imu.md](imu.md) | HI229 与 BNO08X 收成同一种 `imu_dgram_t`。 |
| [time-sync.md](time-sync.md) | SNTP 步进墙上时钟，样本上另盖 Wi-Fi TSF。 |
| [imu-streaming.md](imu-streaming.md) | 环形缓冲里的帧经 UDP 18891 发出。 |
| [wifi.md](wifi.md) | 只做 STA；连上之后才投递对时和发现。 |
| [ble.md](ble.md) | GATT 写入 SSID / PSK，并置位让系统循环去重连。 |
| [api.md](api.md) | 端口 18888 上的 JSON 控制面，外加一条调试 WebSocket。 |
| [discovery.md](discovery.md) | UDP 18889 上用应答的源地址覆盖 data host。 |
| [nvs.md](nvs.md) | namespace `var` 的键；发现结果和升级 URL 不落盘。 |
| [led.md](led.md) | 一颗 LED 上的自动灯语，以及 REST 如何抢走它。 |
| [button.md](button.md) | 单击翻转运行态，长按进入只靠按钮唤醒的深睡。 |
| [battery.md](battery.md) | 有人读时才采样分压，不驱动电源状态机。 |
| [power-management.md](power-management.md) | 待机、推流、省电和深睡时分别打开或关掉哪些外设。 |
| [ota.md](ota.md) | REST 带来的 URL 交给 `esp_https_ota`，启动后 3 秒确认镜像。 |

## 缺口

无。`nvs.md`、`power-management.md`、`led.md`、`imu.md`、`time-sync.md`、`button.md`、`imu-streaming.md`、`ble.md`、`wifi.md`、`api.md`、`discovery.md`、`events.md`、`boot-and-tasks.md`、`ota.md`、`battery.md`、`global-state.md`、`hardware.md` 都在，没有缺篇，也没有只剩标题的提纲。
