//
// Created by liyutong on 2024/4/30.

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "battery.h"
#include "modelspec.h"

static const char *TAG = "battery         ";

static bool cali_enable = false;

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
#endif

    if (ret == ESP_OK) {
        cali_enable = true;
        adc1_cali_handle = handle;
        ESP_LOGI(TAG, "ADC calibration success");
    } else {
        cali_enable = false;
        ESP_LOGW(TAG, "Calibration not supported or failed, skip software calibration");
    }

    return cali_enable;
}

/**
 * @brief Initialize battery management
 * @return ESP_OK on success, ESP_FAIL on error
**/
esp_err_t battery_msp_init() {

    /** Init ADC_EN GPIO **/
#ifdef CONFIG_BATTERY_EN_PIN
    gpio_config_t io_config = {
        .pin_bit_mask = (1ull << CONFIG_BATTERY_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);
#endif

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, CONFIG_BATTERY_READ_ADC_CHANNEL, &chan_config));

    adc_calibration_init(ADC_UNIT_1, CONFIG_BATTERY_READ_ADC_CHANNEL, ADC_ATTEN_DB_12);

    /** Self-Test **/
    int lvl = battery_read_level();
    ESP_LOGI(TAG, "battery level: %d", lvl);
    return ESP_OK;
}

/**
 * @brief Read battery level
 * @return mV
**/
int battery_read_level() {
#ifdef CONFIG_BATTERY_EN_PIN
    gpio_set_level(CONFIG_BATTERY_EN_PIN, CONFIG_BATTERY_EN_VALUE);
#endif
    battery_delay_ms(50);

    int adc_raw;
    uint32_t voltage;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, CONFIG_BATTERY_READ_ADC_CHANNEL, &adc_raw));
    if (cali_enable) {
        int cali_voltage;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &cali_voltage));
        voltage = (uint32_t)cali_voltage;
        ESP_LOGD(TAG, "cali data: %" PRIu32 " mV", voltage);
    } else {
        voltage = adc_raw * 1100 / 4096;
    }

#ifdef CONFIG_BATTERY_EN_PIN
    gpio_set_level(CONFIG_BATTERY_EN_PIN, !CONFIG_BATTERY_EN_VALUE);

    /**
    It goes like this
    VBAT
    +
    |
    |
    [R1] 15kΩ
    |
    |---- ADC_CHANNEL
    |
    [R2] 10kΩ
    |
    \
    |---- ADC_EN
    /
    |
    -
    GND
     **/

    voltage = voltage / 2 * 5; // the voltage divider ratio is 2
#endif
    return (int) voltage;
}
