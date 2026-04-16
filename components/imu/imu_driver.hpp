#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "imu_types.hpp"

class ImuDriver {
public:
    virtual ~ImuDriver() = default;

    virtual esp_err_t init(int32_t target_fps) = 0;
    virtual esp_err_t read(ImuDatagram& out) = 0;
    virtual esp_err_t toggle(bool enable) = 0;
    virtual bool is_enabled() const = 0;
    virtual esp_err_t self_test() = 0;
    virtual void soft_reset() = 0;
    virtual void hard_reset() = 0;
    virtual void buffer_reset() = 0;
    virtual int64_t get_delay_us() const = 0;

    // Debug/diagnostic read
    virtual size_t read_bytes(uint8_t* out, size_t len) = 0;

    bool is_initialized() const { return initialized_; }

    ImuMux mux() const { return mux_; }
    void set_mux(ImuMux m) { mux_ = m; }

    ImuStatus status() const { return status_; }
    int32_t target_fps() const { return target_fps_; }

protected:
    bool initialized_ = false;
    bool enabled_ = false;
    ImuStatus status_ = ImuStatus::UNKNOWN;
    ImuMux mux_ = ImuMux::IDLE;
    int32_t target_fps_ = 100;
    uint64_t seq_ = 0;
    SemaphoreHandle_t mutex_ = nullptr;
};

// Factory: returns a driver based on Kconfig selection
ImuDriver* create_imu_driver(int32_t target_fps);

// Global IMU instance pointer (replaces old g_imu interface struct)
extern ImuDriver* g_imu_driver;
