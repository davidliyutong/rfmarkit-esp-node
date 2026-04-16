# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add git tag to firmware version
EXECUTE_PROCESS(COMMAND git describe --tags --abbrev=0
    TIMEOUT 5
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Check if GIT_VERSION is empty, if so set it to default value
if ("${GIT_VERSION}" STREQUAL "")
    set(GIT_VERSION "3.3.2")
endif ()

MESSAGE(STATUS "building from git tag ${GIT_VERSION}")
add_definitions(-DCONFIG_FIRMWARE_VERSION=\"${GIT_VERSION}\")

# Introduce used components -- all IMU libs are registered;
# Kconfig selection determines which one is actually linked.
set(
    EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/lib/bno08x
    ${CMAKE_SOURCE_DIR}/lib/hi229
    ${CMAKE_SOURCE_DIR}/lib/battery
    ${CMAKE_SOURCE_DIR}/lib/ring_buf
    ${CMAKE_SOURCE_DIR}/lib/libudp
    ${CMAKE_SOURCE_DIR}/lib/spatial
    ${CMAKE_SOURCE_DIR}/components
    ${CMAKE_SOURCE_DIR}/third_party/esp32_BNO08x
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rfmarkit-esp-node)
#idf_build_set_property(COMPILE_OPTIONS "-O3" APPEND)