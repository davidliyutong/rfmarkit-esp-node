idf_component_register(
        SRCS imu.cpp imu_factory.cpp
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        REQUIRES sys
        PRIV_REQUIRES bno08x hi229 esp32_BNO08x
)
