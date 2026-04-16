#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

class Battery {
public:
    esp_err_t init();
    int read_level_mv();

private:
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t cali_handle_ = nullptr;
    bool cali_enabled_ = false;

    bool calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten);
};

// C-compatible wrappers
extern "C" {
esp_err_t battery_msp_init(void);
int battery_read_level(void);
}
