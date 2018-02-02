cmake_minimum_required(VERSION 2.8)
project(hello_pipline)

INCLUDE(FindPkgConfig)

pkg_check_modules(GSTMM REQUIRED gstreamermm-1.0)
include_directories(${GSTMM_INCLUDE_DIRS})
link_directories(${GSTMM_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${GSTMM_LIBRARIES})

