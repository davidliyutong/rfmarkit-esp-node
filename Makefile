.PHONY: setup update set_hardware_v1 set_hardware_v2 set_hardware_v3 build flash monitor flash-monitor clean fullclean menuconfig help

# ---------- ESP-IDF local installation ----------
IDF_VERSION    ?= v5.5.3
IDF_PATH       := $(CURDIR)/.esp-idf
IDF_TOOLS_PATH := $(CURDIR)/.espressif

export IDF_PATH
export IDF_TOOLS_PATH

SHELL := /bin/bash

# Activate ESP-IDF environment (stdout silenced, errors still visible)
IDF_ACTIVATE := . $(IDF_PATH)/export.sh > /dev/null 2>&1 &&

# ---------- Setup ----------

# Clone ESP-IDF and install toolchains for esp32 + esp32s3
setup:
	@if [ ! -d "$(IDF_PATH)" ]; then \
		echo "Cloning ESP-IDF $(IDF_VERSION)..."; \
		git clone -b $(IDF_VERSION) --recursive https://github.com/espressif/esp-idf.git $(IDF_PATH); \
	else \
		echo "ESP-IDF already present at $(IDF_PATH)"; \
	fi
	@echo "Installing toolchain (esp32, esp32s3, esp32c5)..."
	@$(IDF_PATH)/install.sh esp32,esp32s3,esp32c5

# Pull the configured IDF_VERSION and reinstall tools
update:
	cd $(IDF_PATH) && git fetch && git checkout $(IDF_VERSION) && git submodule update --init --recursive
	@$(IDF_PATH)/install.sh esp32,esp32s3,esp32c5

# ---------- Build targets ----------

# Hardware v1: ESP32 + HI229
set_hardware_v1:
	$(IDF_ACTIVATE) idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v1" set-target esp32

# Hardware v2: ESP32-S3 + BNO08X
set_hardware_v2:
	$(IDF_ACTIVATE) idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v2" set-target esp32s3

# Hardware v3: ESP32-C5 + BNO08X
set_hardware_v3:
	$(IDF_ACTIVATE) idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.v3" set-target esp32c5

build:
	$(IDF_ACTIVATE) idf.py build

flash:
	$(IDF_ACTIVATE) idf.py flash

monitor:
	$(IDF_ACTIVATE) idf.py monitor

flash-monitor:
	$(IDF_ACTIVATE) idf.py flash monitor

clean:
	$(IDF_ACTIVATE) idf.py clean

fullclean:
	$(IDF_ACTIVATE) idf.py fullclean

menuconfig:
	$(IDF_ACTIVATE) idf.py menuconfig

# ---------- Help ----------

help:
	@echo "Usage:"
	@echo "  make setup             - Download and install ESP-IDF locally"
	@echo "  make update            - Update local ESP-IDF to IDF_VERSION"
	@echo "  make set_hardware_v1   - Configure for Hardware v1 (ESP32 + HI229)"
	@echo "  make set_hardware_v2   - Configure for Hardware v2 (ESP32-S3 + BNO08X)"
	@echo "  make set_hardware_v3   - Configure for Hardware v3 (ESP32-C5 + BNO08X)"
	@echo "  make build             - Build firmware"
	@echo "  make flash             - Flash to device"
	@echo "  make monitor           - Serial monitor"
	@echo "  make flash-monitor     - Flash and monitor"
	@echo "  make menuconfig        - Open config menu"
	@echo "  make clean             - Clean build"
	@echo "  make fullclean         - Full clean (removes sdkconfig)"
	@echo ""
	@echo "Environment:"
	@echo "  IDF_VERSION    = $(IDF_VERSION)"
	@echo "  IDF_PATH       = $(IDF_PATH)"
	@echo "  IDF_TOOLS_PATH = $(IDF_TOOLS_PATH)"
