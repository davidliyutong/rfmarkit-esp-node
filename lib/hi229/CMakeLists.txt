if(CONFIG_IMU_SENSOR_HI229)
    set(sources hi229_driver.cpp hi229_serial.c)
else()
    set(sources "")
endif()

idf_component_register(
        SRCS ${sources}
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        PRIV_REQUIRES imu driver esp_wifi sys
)
