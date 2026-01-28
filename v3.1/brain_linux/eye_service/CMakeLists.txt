cmake_minimum_required(VERSION 3.16)
project(eye_service LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# Source files
set(EYE_SERVICE_SOURCES
    main.cpp
    eye_animator.cpp
    eye_renderer.cpp
    gc9d01_dualeye_spi.cpp
    gpio/gpio_sysfs.cpp
)

# Common sources (CRC)
set(COMMON_SOURCES
    ${COMMON_INCLUDE_DIR}/crc16_ccitt_false.c
)

# Executable
add_executable(eye_service
    ${EYE_SERVICE_SOURCES}
    ${COMMON_SOURCES}
)

# Include directories
target_include_directories(eye_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gpio
    ${COMMON_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(eye_service PRIVATE
    Threads::Threads
)

# Compiler warnings
target_compile_options(eye_service PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>
)

# Install
install(TARGETS eye_service
    RUNTIME DESTINATION bin
)
