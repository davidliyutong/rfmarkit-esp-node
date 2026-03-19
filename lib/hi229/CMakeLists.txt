if(CONFIG_IMU_SENSOR_HI229)
    FILE(GLOB_RECURSE sources *.c)
else()
    set(sources "")
endif()

idf_component_register(
        SRCS ${sources}
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        PRIV_REQUIRES imu driver esp_wifi
)
