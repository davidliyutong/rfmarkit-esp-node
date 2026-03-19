idf_component_register(
        SRCS imu.c
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        REQUIRES sys
        PRIV_REQUIRES bno08x hi229
)
