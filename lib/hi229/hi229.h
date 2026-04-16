#ifndef _HI229_H
#define _HI229_H

#include "imu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Exposed API **/
void hi229_interface_init(imu_interface_t *p_interface, imu_config_t *p_config);

#ifdef __cplusplus
}
#endif

#endif
