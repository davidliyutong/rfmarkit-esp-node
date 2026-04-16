#include <sys/time.h>
#include <cstring>
#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "bno08x_driver.hpp"

static const char* TAG = "imu[bno08x]";

esp_err_t Bno08xDriver::configure_reports() {
    uint32_t interval_us = 1000000 / target_fps_;

#if CONFIG_USE_LINEAR_ACCELERATION
    if (!device_.rpt.linear_accelerometer.enable(interval_us)) {
        ESP_LOGE(TAG, "failed to enable linear accelerometer");
        return ESP_FAIL;
    }
#else
    if (!device_.rpt.accelerometer.enable(interval_us)) {
        ESP_LOGE(TAG, "failed to enable accelerometer");
        return ESP_FAIL;
    }
#endif

#if CONFIG_USE_GYRO
    if (!device_.rpt.cal_gyro.enable(interval_us)) {
        ESP_LOGE(TAG, "failed to enable gyroscope");
        return ESP_FAIL;
    }
#endif

    if (!device_.rpt.rv.enable(interval_us)) {
        ESP_LOGE(TAG, "failed to enable rotation vector");
        return ESP_FAIL;
    }

    constexpr uint32_t SLOW_INTERVAL_US = 500000; // 2Hz
    device_.rpt.step_counter.enable(SLOW_INTERVAL_US);
    device_.rpt.stability_classifier.enable(SLOW_INTERVAL_US);

    enabled_ = true;
    return ESP_OK;
}

esp_err_t Bno08xDriver::init(int32_t target_fps) {
    target_fps_ = target_fps;
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "failed to create mutex");
        status_ = ImuStatus::FAIL;
        return ESP_FAIL;
    }

    if (!device_.initialize()) {
        ESP_LOGE(TAG, "failed to initialize BNO08x");
        status_ = ImuStatus::FAIL;
        return ESP_FAIL;
    }

    status_ = ImuStatus::READY;
    initialized_ = true;
    return configure_reports();
}

esp_err_t Bno08xDriver::read(ImuDatagram& out) {
    int64_t now = esp_timer_get_time();

    if (device_.data_available()) {
        // Quaternion: library uses {real, i, j, k}, we store as {w, x, y, z}
        if (device_.rpt.rv.has_new_data()) {
            bno08x_quat_t q = device_.rpt.rv.get_quat();
            out.imu.quat[0] = q.real;
            out.imu.quat[1] = q.i;
            out.imu.quat[2] = q.j;
            out.imu.quat[3] = q.k;
        }

        // Accelerometer
#if CONFIG_USE_LINEAR_ACCELERATION
        if (device_.rpt.linear_accelerometer.has_new_data()) {
            bno08x_accel_t a = device_.rpt.linear_accelerometer.get();
            out.imu.acc[0] = a.x;
            out.imu.acc[1] = a.y;
            out.imu.acc[2] = a.z;
        }
#else
        if (device_.rpt.accelerometer.has_new_data()) {
            bno08x_accel_t a = device_.rpt.accelerometer.get();
            out.imu.acc[0] = a.x;
            out.imu.acc[1] = a.y;
            out.imu.acc[2] = a.z;
        }
#endif

        // Gyroscope
#if CONFIG_USE_GYRO
        if (device_.rpt.cal_gyro.has_new_data()) {
            bno08x_gyro_t g = device_.rpt.cal_gyro.get();
            out.imu.gyr[0] = g.x;
            out.imu.gyr[1] = g.y;
            out.imu.gyr[2] = g.z;
        }
#endif

        // TSF timestamp
        out.tsf_ts_us = esp_wifi_get_tsf_time(WIFI_IF_STA);
        out.buffer_delay_us = static_cast<int32_t>(esp_timer_get_time() - now);

        // Unix timestamp
        struct timeval tv_now = {};
        gettimeofday(&tv_now, nullptr);
        out.dev_ts_us = static_cast<int64_t>(tv_now.tv_sec) * 1000000LL + static_cast<int64_t>(tv_now.tv_usec);

        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t Bno08xDriver::toggle(bool enable) {
    if (enable == enabled_) {
        ESP_LOGW(TAG, "imu already %s", enable ? "enabled" : "disabled");
        return ESP_OK;
    }

    bool success = false;
    if (enable) {
        success = device_.hard_reset();
        if (success) {
            success = device_.initialize();
            if (success) {
                esp_err_t err = configure_reports();
                success = (err == ESP_OK);
            }
        }
        if (success) enabled_ = true;
    } else {
        success = device_.sleep();
        if (success) enabled_ = false;
    }

    return success ? ESP_OK : ESP_FAIL;
}

bool Bno08xDriver::is_enabled() const {
    return enabled_;
}

esp_err_t Bno08xDriver::self_test() {
    return ESP_OK;
}

void Bno08xDriver::soft_reset() {
    device_.soft_reset();
}

void Bno08xDriver::hard_reset() {
    device_.hard_reset();
}

void Bno08xDriver::buffer_reset() {
    // No buffer to reset with the library-based driver
}

int64_t Bno08xDriver::get_delay_us() const {
    return 0;
}

size_t Bno08xDriver::read_bytes(uint8_t* out, size_t len) {
    ImuDatagram dgram = {};
    if (read(dgram) == ESP_OK) {
        int written = snprintf(
            reinterpret_cast<char*>(out), len,
            "[%+.2f %+.2f %+.2f %+.2f], [%+.2f %+.2f %+.2f]",
            dgram.imu.quat[0], dgram.imu.quat[1], dgram.imu.quat[2], dgram.imu.quat[3],
            dgram.imu.eul[0], dgram.imu.eul[1], dgram.imu.eul[2]
        );
        return (written > 0) ? static_cast<size_t>(written) : 0;
    }
    return 0;
}

// C-compatible shim: old imu_interface_t wrapper
// This allows the existing C code (sys, apps, etc.) to keep using g_imu
// until those components are converted to C++.

extern "C" {
#include "imu.h"
}

static imu_t s_imu_compat = {};
static Bno08xDriver* s_driver_ptr = nullptr;

static esp_err_t compat_init(imu_t* p_imu, imu_config_t* p_config) {
    (void)p_imu;
    (void)p_config;
    return ESP_OK; // already initialized
}

static esp_err_t compat_read(imu_t*, imu_dgram_t* out, bool) {
    if (!s_driver_ptr) return ESP_FAIL;
    // The C and C++ imu_dgram_t/ImuDatagram are layout-compatible
    return s_driver_ptr->read(*reinterpret_cast<ImuDatagram*>(out));
}

static esp_err_t compat_toggle(imu_t* p_imu, bool enable) {
    if (!s_driver_ptr) return ESP_FAIL;
    esp_err_t ret = s_driver_ptr->toggle(enable);
    p_imu->enabled = s_driver_ptr->is_enabled();
    return ret;
}

static int compat_enabled(imu_t*) {
    return s_driver_ptr ? (s_driver_ptr->is_enabled() ? 1 : 0) : 0;
}

static esp_err_t compat_self_test(imu_t*) {
    return s_driver_ptr ? s_driver_ptr->self_test() : ESP_FAIL;
}

static void compat_soft_reset(imu_t*) {
    if (s_driver_ptr) s_driver_ptr->soft_reset();
}

static void compat_hard_reset(imu_t*) {
    if (s_driver_ptr) s_driver_ptr->hard_reset();
}

static void compat_buffer_reset(imu_t*) {
    if (s_driver_ptr) s_driver_ptr->buffer_reset();
}

static int64_t compat_get_delay_us(imu_t*) {
    return s_driver_ptr ? s_driver_ptr->get_delay_us() : 0;
}

static size_t compat_read_bytes(imu_t*, uint8_t* out, size_t len) {
    return s_driver_ptr ? s_driver_ptr->read_bytes(out, len) : 0;
}

static esp_err_t compat_write_bytes(imu_t*, void*, size_t) {
    return ESP_OK;
}

extern "C" void bno08x_interface_init(imu_interface_t* p_interface, imu_config_t* p_config) {
    static Bno08xDriver s_driver;
    s_driver_ptr = &s_driver;

    s_imu_compat.target_fps = p_config->target_fps;
    s_driver.init(p_config->target_fps);
    s_imu_compat.initialized = s_driver.is_initialized();
    s_imu_compat.enabled = s_driver.is_enabled();
    s_imu_compat.status = static_cast<imu_status_t>(s_driver.status());

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
