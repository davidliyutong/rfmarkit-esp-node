#ifndef BATTERY_
#define BATTERY_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_msp_init(void);
int battery_read_level(void);

#ifdef __cplusplus
}
#endif

#endif
