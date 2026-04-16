idf_component_register(
        SRCS battery.cpp
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        REQUIRES esp_adc esp_driver_gpio
)
