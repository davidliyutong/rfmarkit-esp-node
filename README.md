# RFMarkIt Firmware

This is the firmware for RFMarkIt project, which is a project for a active marker system for motion capture / 6DOF tracking.

![](./docs/imgs/system.jpg)

![](./docs/imgs/hardware_v1.jpg)

For more details, visit [our website](https://sites.google.com/view/markit-virat/home)

## Local Development Setup

### Prerequisites

- **CMake, Ninja** — `brew install cmake ninja dfu-util` (macOS) or via your package manager
- **Python 3.9+**
- **[uv](https://docs.astral.sh/uv/)** and **[direnv](https://direnv.net/)** (optional, for automatic env activation):

```shell
brew install uv direnv    # macOS
```

- **Apple Silicon** — if you hit xtensa toolchain errors, install Rosetta: `/usr/sbin/softwareupdate --install-rosetta --agree-to-license`

### Setup

Bootstrap a project-local ESP-IDF installation (cloned into `.esp-idf/`, tools into `.espressif/`):

```shell
make setup              # one-time: clones ESP-IDF and installs toolchains
```

This does not touch your system environment. All subsequent `make` targets automatically activate the local ESP-IDF.

To pin a different ESP-IDF version:

```shell
make setup IDF_VERSION=v4.4.4
```

Optionally set up Python helpers and direnv:

```shell
uv sync                 # install Python dependencies (pyserial, etc.)
direnv allow            # auto-activate ESP-IDF env when entering the directory
```

### Recommended IDE

VSCode with the [ESP-IDF Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).

### Serial Port

- macOS: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- Linux: `/dev/ttyUSB*` (may need `sudo usermod -aG dialout $USER`)

## Get Started

Choose your hardware target. v1 uses ESP32 + HI229, v2 uses ESP32-S3 + BNO085:

```shell
make set_hardware_v1    # or: idf.py set-target esp32
make set_hardware_v2    # or: idf.py set-target esp32s3
```

To change other settings (e.g. use linear acceleration):

```shell
make menuconfig         # or: idf.py menuconfig
```

Build:

```shell
make build              # or: idf.py build
```

Flash and monitor:

```shell
make flash-monitor      # or: idf.py -p <PORT> flash monitor
```

Run `make help` for all available targets.

## Developers' Guide

The project is organized as follows:

- `components` system components
    - `components/apps` applications that runs with RTOS
    - `components/ble` Bluetooth Low Energy(BLE) components
    - `components/blink` functions to operate LED
    - `components/imu` imu interface
    - `components/rest_controller` RESTful API controller
    - `components/sys` supporting modules for system
- `docs` documents
- `include` global headers
- `lib` common libraries
    - `lib/battery` battery
    - `lib/bno085` bno085 library
    - `lib/hi229` hi229 library
    - `lib/libtcp` tcp library
    - `lib/libudp` udp library
    - `lib/ring_buf` ring buffer library
    - `lib/spatial` spatial math library
- `main` entrypoint of firmware
- `scripts` helper scripts
- `tests` function tests

## Operators' Guide

The detailed guide can be found in [docs/manual.md](docs/manual.md)

### List of API

The API can be used against `http://<UNIT_IP>:18888/` endpiont

| NEW API           | PATH                    | Type            | Function                                                 |
|-------------------|-------------------------|-----------------|----------------------------------------------------------|
| system_info       | /v1/system/info         | `[get]       `  | ping,id,ver,time                                         |
| system_power      | /v1/system/power        | `[post]      `  | reboot, shutdown,                                        |
| system_upgrade    | /v1/system/upgrade      | `[get\|post]  ` | update                                                   |
| system_selftest   | /v1/system/selftest     | `[post]      `  | self_test                                                |
| system_power_mgmt | /v1/system/power_mgmt   | `[post]      `  | always_on, cancel_always_on                              |
| nvs_variable      | /v1/nvs/variable/<name> | `[get\|post]  ` | varset,varget                                            |
| imu_calibrate     | /v1/imu/calibrate       | `[post]      `  | imu_cali_reset, imu_cali_acc, imu_cali_mag               |
| imu_toggle        | /v1/imu/toggle          | `[post]      `  | imu_enable,imu_disable,                                  |
| imu_status        | /v1/imu/status          | `[get]       `  | imu_status imu_imm                                       |
| imu_debug_toggle  | /v1/imu/debug/toggle    | `[post]      `  | toggle debug mode and disconnect monitor                 |
| imu_debug_socket  | /v1/imu/debug/socket    | `[ws]        `  | imu_debug imu_setup                                      |
| blink_configure   | /v1/blink/configure     | `[get\|post]  ` | blink_set, blink_get, auto/manual                        |
| blink_toggle      | /v1/blink/toggle        | `[get\|post]  ` | blink_start, blink_stop, blink_mute, also get led status |
| operation_mode    | /v1/operation/mode      | `[get\|post]  ` | start stop                                               |

