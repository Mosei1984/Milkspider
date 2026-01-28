cmake_minimum_required(VERSION 3.16)
project(SpiderRobot_v31 VERSION 3.1.0 LANGUAGES C CXX)

# Build options
option(BUILD_BRAIN_LINUX "Build Linux components (brain_daemon, eye_service)" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Common include directory
set(COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

# Build Linux components
if(BUILD_BRAIN_LINUX)
    add_subdirectory(brain_linux)
endif()

# Summary
message(STATUS "")
message(STATUS "Spider Robot v3.1 Build Configuration")
message(STATUS "======================================")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:       ${CMAKE_C_COMPILER}")
message(STATUS "  C++ Compiler:     ${CMAKE_CXX_COMPILER}")
message(STATUS "  Common headers:   ${COMMON_INCLUDE_DIR}")
message(STATUS "")
