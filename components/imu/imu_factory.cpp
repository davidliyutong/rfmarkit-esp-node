#include "imu_driver.hpp"

#if CONFIG_IMU_SENSOR_BNO08X
#include "bno08x_driver.hpp"
#elif CONFIG_IMU_SENSOR_HI229
#include "hi229_driver.hpp"
#endif

ImuDriver* g_imu_driver = nullptr;

#if CONFIG_IMU_SENSOR_BNO08X
static Bno08xDriver s_bno08x_driver;
#elif CONFIG_IMU_SENSOR_HI229
static Hi229Driver s_hi229_driver;
#endif

ImuDriver* create_imu_driver(int32_t target_fps) {
#if CONFIG_IMU_SENSOR_BNO08X
    s_bno08x_driver.init(target_fps);
    g_imu_driver = &s_bno08x_driver;
#elif CONFIG_IMU_SENSOR_HI229
    s_hi229_driver.init(target_fps);
    g_imu_driver = &s_hi229_driver;
#else
    #error "No IMU sensor selected in Kconfig"
#endif
    return g_imu_driver;
}
