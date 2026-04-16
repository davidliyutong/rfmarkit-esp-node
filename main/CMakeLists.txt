FILE(GLOB_RECURSE sources ./*.cpp)
idf_component_register(
        SRCS ${sources}
        INCLUDE_DIRS . ${CMAKE_SOURCE_DIR}/include
)
