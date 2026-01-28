cmake_minimum_required(VERSION 3.16)
project(brain_daemon_v31 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common include directory
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")

# Source files
set(SOURCES
    main.cpp
    mailbox.cpp
    shared_memory.cpp
    eye_client.cpp
    distance_sensor.cpp
    serial_control.cpp
    ${COMMON_DIR}/crc16_ccitt_false.c
)

# Executable
add_executable(brain_daemon ${SOURCES})

# Include directories
target_include_directories(brain_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${COMMON_DIR}
)

# No external dependencies - using minimal built-in SHA1

# Compiler warnings
target_compile_options(brain_daemon PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2 -s>
    $<$<CONFIG:Debug>:-O0 -g>
)

# Install
install(TARGETS brain_daemon RUNTIME DESTINATION bin)

# Print build info
message(STATUS "Building brain_daemon for Spider Robot v3.1")
message(STATUS "Common headers: ${COMMON_DIR}")
