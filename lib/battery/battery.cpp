#include <cinttypes>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "battery.hpp"
#include "modelspec.h"

static const char* TAG = "battery         ";

static Battery s_battery;

bool Battery::calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten) {
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle_);
#else
    (void)unit;
    (void)channel;
    (void)atten;
#endif

    if (ret == ESP_OK) {
        cali_enabled_ = true;
        ESP_LOGI(TAG, "ADC calibration success");
    } else {
        cali_enabled_ = false;
        ESP_LOGW(TAG, "Calibration not supported or failed, skip software calibration");
    }
    return cali_enabled_;
}

esp_err_t Battery::init() {
#ifdef CONFIG_BATTERY_EN_PIN
    gpio_config_t io_config = {
        .pin_bit_mask = (1ull << CONFIG_BATTERY_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config);
#endif

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = {},
        .ulp_mode = {},
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, CONFIG_BATTERY_READ_ADC_CHANNEL, &chan_config));

    calibration_init(ADC_UNIT_1, CONFIG_BATTERY_READ_ADC_CHANNEL, ADC_ATTEN_DB_12);

    int lvl = read_level_mv();
    ESP_LOGI(TAG, "battery level: %d", lvl);
    return ESP_OK;
}

int Battery::read_level_mv() {
#ifdef CONFIG_BATTERY_EN_PIN
    constexpr int BATTERY_EN_VALUE = 1;
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_BATTERY_EN_PIN), BATTERY_EN_VALUE);
#endif
    vTaskDelay(50 / portTICK_PERIOD_MS);

    int adc_raw;
    uint32_t voltage;

    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, CONFIG_BATTERY_READ_ADC_CHANNEL, &adc_raw));
    if (cali_enabled_) {
        int cali_voltage;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle_, adc_raw, &cali_voltage));
        voltage = static_cast<uint32_t>(cali_voltage);
        ESP_LOGD(TAG, "cali data: %" PRIu32 " mV", voltage);
    } else {
        voltage = adc_raw * 1100 / 4096;
    }

#ifdef CONFIG_BATTERY_EN_PIN
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_BATTERY_EN_PIN), !BATTERY_EN_VALUE);
    voltage = voltage / 2 * 5;
#endif
    return static_cast<int>(voltage);
}

// C-compatible wrappers
extern "C" {

esp_err_t battery_msp_init(void) {
    return s_battery.init();
}

int battery_read_level(void) {
    return s_battery.read_level_mv();
}

} // extern "C"
