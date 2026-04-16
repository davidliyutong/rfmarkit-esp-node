#pragma once

#include <cstdint>

struct ImuData {
    uint32_t id = 0;
    float acc[3] = {};
    float gyr[3] = {};
    float mag[3] = {};
    float eul[3] = {};
    float quat[4] = {};
    float pressure = 0;
    uint32_t imu_ts_ms = 0;
};

struct ImuDatagram {
    ImuData imu;
    int64_t dev_ts_us = 0;
    int64_t tsf_ts_us = 0;
    uint32_t seq = 0;
    int32_t buffer_delay_us = 0;
};

// Note: Do not define C-compatible type aliases here to avoid
// conflicts with imu.h when both headers are included in C++ files.

enum class ImuStatus : int8_t {
    UNKNOWN = -1,
    FAIL = 0,
    READY = 1,
};

enum class ImuMux : uint8_t {
    IDLE = 0,
    STREAM = 1,
    DEBUG = 2,
};
