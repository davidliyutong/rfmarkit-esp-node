#pragma once

#include "imu_driver.hpp"
#include "driver/uart.h"
#include "driver/gpio.h"

extern "C" {
#include "hi229_serial.h"
}

struct Hi229Config {
    uart_port_t port;
    int baud;
    gpio_num_t ctrl_pin;
    gpio_num_t rx_pin;
    gpio_num_t tx_pin;
    gpio_num_t rts_pin;
    gpio_num_t cts_pin;
    gpio_num_t sync_in_pin;
    gpio_num_t sync_out_pin;
};

class Hi229Driver : public ImuDriver {
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

    void set_config(const Hi229Config& cfg) { config_ = cfg; }
    const Hi229Config& config() const { return config_; }

private:
    Hi229Config config_ = {};
    raw_t raw_ = {};
    ch_imu_data_t raw_snapshot_ = {};
    int64_t raw_timestamp_ = 0;
    TaskHandle_t poll_task_hdl_ = nullptr;
    EventGroupHandle_t event_group_ = nullptr;

    void msp_init();
    static void poll_task_entry(void* arg);

    static constexpr EventBits_t EV_DATA_READY = BIT0;
};
