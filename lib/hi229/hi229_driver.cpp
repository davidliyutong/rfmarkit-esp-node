#include <sys/time.h>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "driver/rtc_io.h"

#include "hi229_driver.hpp"

extern "C" {
#include "hi229_serial.h"
}

static const char* TAG = "imu[hi229]";

// Constants from old hi229.h
static constexpr int HI229_UART_RX_BUF_LEN = 5120;
static constexpr int HI229_UART_TX_BUF_LEN = 0;
static constexpr int HI229_ENABLE_LVL = 0;
static constexpr int HI229_DISABLE_LVL = 1;
static constexpr int HI229_SELF_TEST_RETRY = 4;
static constexpr int HI229_BLOCK_READ_NUM = 32;
static constexpr int HI229_MAX_READ_NUM = 512;

void Hi229Driver::msp_init() {
    ESP_LOGI(TAG, "UART .port=%d, .rx_pin=%d, .tx_pin=%d",
             config_.port, config_.rx_pin, config_.tx_pin);

    uart_config_t uart_config = {
        .baud_rate = config_.baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_driver_install(config_.port, HI229_UART_RX_BUF_LEN, HI229_UART_TX_BUF_LEN, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(config_.port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(config_.port, config_.tx_pin, config_.rx_pin, config_.rts_pin, config_.cts_pin));

    gpio_config_t ctrl_pin_conf = {
        .pin_bit_mask = (1ULL << config_.ctrl_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ctrl_pin_conf);
    gpio_set_level(config_.ctrl_pin, 0);

    gpio_config_t sync_out_conf = {
        .pin_bit_mask = (1ULL << config_.sync_out_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sync_out_conf);
    gpio_set_level(config_.sync_out_pin, 0);

    gpio_config_t sync_in_conf = {
        .pin_bit_mask = (1ULL << config_.sync_in_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sync_in_conf);

    enabled_ = true;
    bool valid = rtc_gpio_is_valid_gpio(config_.ctrl_pin);
    if (valid) {
        ESP_LOGI(TAG, "GPIO.%d is valid rtc gpio", config_.ctrl_pin);
    } else {
        ESP_LOGW(TAG, "GPIO.%d is not valid rtc gpio", config_.ctrl_pin);
    }
}

void Hi229Driver::poll_task_entry(void* arg) {
    auto* self = static_cast<Hi229Driver*>(arg);
    uint8_t data[HI229_BLOCK_READ_NUM] = {};

    while (true) {
        int len = uart_read_bytes(self->config_.port, data, HI229_BLOCK_READ_NUM, 0x10);

        for (int idx = 0; idx < len; ++idx) {
            if (ch_serial_input(&self->raw_, data[idx], true) == 1) {
                memcpy(&self->raw_snapshot_, &self->raw_.imu[0], sizeof(ch_imu_data_t));
                self->raw_timestamp_ = esp_timer_get_time();
                xEventGroupSetBits(self->event_group_, EV_DATA_READY);
            }
        }

        if (len < HI229_BLOCK_READ_NUM) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

esp_err_t Hi229Driver::init(int32_t target_fps) {
    target_fps_ = target_fps;
    enabled_ = false;
    memset(&raw_, 0, sizeof(raw_));

    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "failed to create mutex");
        status_ = ImuStatus::FAIL;
        return ESP_FAIL;
    }
    status_ = ImuStatus::READY;
    event_group_ = xEventGroupCreate();

    msp_init();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    toggle(true);
    xTaskCreate(poll_task_entry, "hi229_poll_task", 4096, this, 7, &poll_task_hdl_);
    initialized_ = true;
    return ESP_OK;
}

esp_err_t Hi229Driver::read(ImuDatagram& out) {
    if (xEventGroupWaitBits(event_group_, EV_DATA_READY, pdFALSE, pdFALSE, 0x10) == EV_DATA_READY) {
        ESP_LOGD(TAG, "IMU data ready, acc_x=%f", raw_snapshot_.acc[0]);
        memcpy(&out.imu, &raw_snapshot_, sizeof(ch_imu_data_t));
        int64_t time_delta = esp_timer_get_time() - raw_timestamp_;

        out.tsf_ts_us = esp_wifi_get_tsf_time(WIFI_IF_STA) - time_delta;

        size_t uart_buffer_len;
        uart_get_buffered_data_len(config_.port, &uart_buffer_len);
        out.buffer_delay_us = static_cast<int32_t>(1000000 * uart_buffer_len / config_.baud);

        struct timeval tv_now = {};
        gettimeofday(&tv_now, nullptr);
        out.dev_ts_us = static_cast<int64_t>(tv_now.tv_sec) * 1000000LL + static_cast<int64_t>(tv_now.tv_usec) - time_delta;

        xEventGroupClearBits(event_group_, EV_DATA_READY);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Hi229Driver::toggle(bool enable) {
    gpio_set_level(config_.ctrl_pin, enable ? HI229_ENABLE_LVL : HI229_DISABLE_LVL);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    uart_flush(config_.port);
    enabled_ = enable;

    int ret = gpio_get_level(config_.ctrl_pin);
    ESP_LOGI(TAG, "hi229 toggle, control_pin(:%d)=%s", config_.ctrl_pin, ret ? "HIGH" : "LOW");
    return ESP_OK;
}

bool Hi229Driver::is_enabled() const {
    return enabled_;
}

esp_err_t Hi229Driver::self_test() {
    uint8_t data[HI229_MAX_READ_NUM] = {};
    size_t index_logged[HI229_SELF_TEST_RETRY] = {};
    size_t index_cursor = 0;

    uart_flush(config_.port);
    int len = uart_read_bytes(config_.port, data, HI229_MAX_READ_NUM, 0x100);
    for (int idx = 0; idx < len - 1; ++idx) {
        if (data[idx] == 0x5A && data[idx + 1] == 0xA5 && index_cursor < HI229_SELF_TEST_RETRY) {
            index_logged[index_cursor++] = idx;
        }
    }
    ESP_LOGI(TAG, "IMU self test finished, addr=[%zu, %zu, %zu, %zu]",
             index_logged[0], index_logged[1], index_logged[2], index_logged[3]);

    if (index_cursor == HI229_SELF_TEST_RETRY) {
        status_ = ImuStatus::READY;
        return ESP_OK;
    }
    status_ = ImuStatus::FAIL;
    return ESP_FAIL;
}

void Hi229Driver::soft_reset() {
    const char* msg = "AT+RST\r\n";
    uart_write_bytes_with_break(config_.port, msg, strlen(msg), 0xF);
    uart_wait_tx_done(config_.port, portMAX_DELAY);
}

void Hi229Driver::hard_reset() {
    toggle(false);
    toggle(true);
}

void Hi229Driver::buffer_reset() {
    memset(&raw_, 0, sizeof(raw_));
    uart_flush(config_.port);
}

int64_t Hi229Driver::get_delay_us() const {
    size_t uart_buffer_len;
    uart_get_buffered_data_len(config_.port, &uart_buffer_len);
    return static_cast<int64_t>(1000000 * uart_buffer_len / config_.baud);
}

size_t Hi229Driver::read_bytes(uint8_t* out, size_t len) {
    return uart_read_bytes(config_.port, out, len, 0x100);
}

// C-compatible shim
extern "C" {
#include "imu.h"
#include "sys.h"
}

static imu_t s_imu_compat = {};
static Hi229Driver s_hi229_driver;

static esp_err_t compat_init(imu_t*, imu_config_t*) { return ESP_OK; }

static esp_err_t compat_read(imu_t*, imu_dgram_t* out, bool) {
    return s_hi229_driver.read(*reinterpret_cast<ImuDatagram*>(out));
}

static esp_err_t compat_toggle(imu_t* p_imu, bool enable) {
    esp_err_t ret = s_hi229_driver.toggle(enable);
    p_imu->enabled = s_hi229_driver.is_enabled();
    return ret;
}

static int compat_enabled(imu_t*) {
    return s_hi229_driver.is_enabled() ? 1 : 0;
}

static esp_err_t compat_self_test(imu_t*) {
    return s_hi229_driver.self_test();
}

static void compat_soft_reset(imu_t*) { s_hi229_driver.soft_reset(); }
static void compat_hard_reset(imu_t*) { s_hi229_driver.hard_reset(); }
static void compat_buffer_reset(imu_t*) { s_hi229_driver.buffer_reset(); }
static int64_t compat_get_delay_us(imu_t*) { return s_hi229_driver.get_delay_us(); }

static size_t compat_read_bytes(imu_t*, uint8_t* out, size_t len) {
    return s_hi229_driver.read_bytes(out, len);
}

static esp_err_t compat_write_bytes(imu_t*, void* in, size_t len) {
    uart_write_bytes_with_break(s_hi229_driver.config().port,
                                static_cast<const char*>(in), len, 0xF);
    return ESP_OK;
}

extern "C" void hi229_interface_init(imu_interface_t* p_interface, imu_config_t* p_config) {
    if (s_imu_compat.initialized) {
        ESP_LOGW(TAG, "IMU already initialized");
        p_interface->p_imu = &s_imu_compat;
        return;
    }

    Hi229Config cfg = {
        .port = static_cast<uart_port_t>(CONFIG_IMU_UART_PORT),
        .baud = g_mcu.imu_baud,
        .ctrl_pin = static_cast<gpio_num_t>(CONFIG_IMU_EN_PIN),
        .rx_pin = static_cast<gpio_num_t>(CONFIG_IMU_RX_PIN),
        .tx_pin = static_cast<gpio_num_t>(CONFIG_IMU_TX_PIN),
        .rts_pin = static_cast<gpio_num_t>(UART_PIN_NO_CHANGE),
        .cts_pin = static_cast<gpio_num_t>(UART_PIN_NO_CHANGE),
        .sync_in_pin = static_cast<gpio_num_t>(CONFIG_IMU_SYNC_IN_PIN),
        .sync_out_pin = static_cast<gpio_num_t>(CONFIG_IMU_SYNC_OUT_PIN),
    };
    s_hi229_driver.set_config(cfg);
    s_hi229_driver.init(p_config->target_fps);

    s_imu_compat.initialized = s_hi229_driver.is_initialized();
    s_imu_compat.enabled = s_hi229_driver.is_enabled();
    s_imu_compat.status = static_cast<imu_status_t>(s_hi229_driver.status());
    s_imu_compat.target_fps = p_config->target_fps;

    p_interface->p_imu = &s_imu_compat;
    p_interface->init = compat_init;
    p_interface->read = compat_read;
    p_interface->toggle = compat_toggle;
    p_interface->enabled = compat_enabled;
    p_interface->self_test = compat_self_test;
    p_interface->soft_reset = compat_soft_reset;
    p_interface->hard_reset = compat_hard_reset;
    p_interface->buffer_reset = compat_buffer_reset;
    p_interface->get_delay_us = compat_get_delay_us;
    p_interface->read_bytes = compat_read_bytes;
    p_interface->write_bytes = compat_write_bytes;
}
