FILE(GLOB_RECURSE sources ./*.cpp)
idf_component_register(
        SRCS ${sources}
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
        REQUIRES sys esp_http_server json imu vfs esp_wifi
)