#pragma once

#include "imu_driver.hpp"

// Suppress legacy imu_config_t typedef from the BNO08x library
// which conflicts with our own imu_config_t in imu.h
#define imu_config_t bno08x_legacy_imu_config_t
#include "BNO08x.hpp"
#undef imu_config_t

class Bno08xDriver : public ImuDriver {
public:
    esp_err_t init(int32_t target_fps) override;
    esp_err_t read(ImuDatagram& out) override;
    esp_err_t toggle(bool enable) override;
    bool is_enabled() const override;
    esp_err_t self_test() override;
    void soft_reset() override;
    void hard_reset() override;
    void buffer_reset() override;
    int64_t get_delay_us() const override;
    size_t read_bytes(uint8_t* out, size_t len) override;

private:
    BNO08x device_;
    esp_err_t configure_reports();
};
