#ifndef _BNO08X_H
#define _BNO08X_H

#include "imu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Exposed API **/
void bno08x_interface_init(imu_interface_t *p_interface, imu_config_t *p_config);

#ifdef __cplusplus
}
#endif

#endif
