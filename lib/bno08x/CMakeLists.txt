if(CONFIG_IMU_SENSOR_BNO08X)
    set(sources bno08x_driver.cpp)
else()
    set(sources "")
endif()

idf_component_register(
        SRCS ${sources}
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        PRIV_REQUIRES imu driver esp_wifi esp32_BNO08x
)
