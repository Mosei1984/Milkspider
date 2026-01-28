cmake_minimum_required(VERSION 3.16)
project(brain_daemon LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# nlohmann/json: try find_package first, fall back to FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Source files
set(BRAIN_DAEMON_SOURCES
    main.cpp
    config_store.cpp
    error_handler.cpp
    eye_client_unix.cpp
    ipc_muscle_rpmsg.cpp
    json_protocol.cpp
    lidar_driver.cpp
    motion_loader.cpp
    obstacle_avoidance.cpp
    boot_demo.cpp
    scan_controller.cpp
    task_manager.cpp
    telemetry.cpp
    ws_server.cpp
)

# Common sources (CRC)
set(COMMON_SOURCES
    ${COMMON_INCLUDE_DIR}/crc16_ccitt_false.c
)

# Executable
add_executable(brain_daemon
    ${BRAIN_DAEMON_SOURCES}
    ${COMMON_SOURCES}
)

# Include directories
target_include_directories(brain_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${COMMON_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(brain_daemon PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Compiler warnings
target_compile_options(brain_daemon PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>
)

# Install
install(TARGETS brain_daemon
    RUNTIME DESTINATION bin
)
