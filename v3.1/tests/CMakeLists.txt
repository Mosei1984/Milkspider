cmake_minimum_required(VERSION 3.16)
project(spider_v31_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Common includes
include_directories(${CMAKE_SOURCE_DIR}/../common)
include_directories(${CMAKE_SOURCE_DIR}/mocks)

# Add common source
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/../common/crc16_ccitt_false.c
)

# Test executables
add_executable(test_crc16 test_crc16.cpp ${COMMON_SOURCES})
add_executable(test_posepacket test_posepacket.cpp ${COMMON_SOURCES})
add_executable(test_interpolator test_interpolator.cpp
    ${CMAKE_SOURCE_DIR}/../muscle_rtos/motion_runtime/interpolator.c
)
add_executable(test_clamp test_clamp.cpp)

# Mock-based tests (for Brain Daemon components)
add_executable(test_json_protocol test_json_protocol.cpp
    ${CMAKE_SOURCE_DIR}/../brain_linux/brain_daemon/json_protocol.cpp
)

# CTest integration
enable_testing()
add_test(NAME CRC16 COMMAND test_crc16)
add_test(NAME PosePacket COMMAND test_posepacket)
add_test(NAME Interpolator COMMAND test_interpolator)
add_test(NAME Clamp COMMAND test_clamp)
add_test(NAME JsonProtocol COMMAND test_json_protocol)
