#include "imu.h"

#if CONFIG_IMU_SENSOR_BNO08X
#include "bno08x.h"
#elif CONFIG_IMU_SENSOR_HI229
#include "hi229.h"
#endif

imu_interface_t g_imu = {0};

void imu_interface_init(imu_interface_t *p_interface, imu_config_t *p_config) {
#if CONFIG_IMU_SENSOR_BNO08X
    bno08x_interface_init(p_interface, p_config);
#elif CONFIG_IMU_SENSOR_HI229
    hi229_interface_init(p_interface, p_config);
#else
    #error "No IMU sensor selected in Kconfig"
#endif
}
