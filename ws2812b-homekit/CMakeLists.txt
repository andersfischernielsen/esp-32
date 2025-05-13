cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_SOURCE_DIR}/components
)

idf_build_set_property(MINIMAL_BUILD ON)
project(scd41-homekit)
