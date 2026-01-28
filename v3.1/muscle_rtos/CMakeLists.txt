# Spider Robot v3.1 Muscle Runtime CMakeLists.txt

# Gather all source files including subdirectories
file(GLOB _SOURCES "src/${RUN_ARCH}/*.c")
file(GLOB _DRIVER_SOURCES "src/${RUN_ARCH}/drivers/*.c")
file(GLOB _SAFETY_SOURCES "src/${RUN_ARCH}/safety/*.c")
file(GLOB _MOTION_SOURCES "src/${RUN_ARCH}/motion_runtime/*.c")

# Combine all sources
set(_ALL_SOURCES 
    ${_SOURCES}
    ${_DRIVER_SOURCES}
    ${_SAFETY_SOURCES}
    ${_MOTION_SOURCES}
)

if (CONFIG_FAST_IMAGE_TYPE STRGREATER "0")
add_compile_definitions(FAST_IMAGE_ENABLE)
endif()

# Include directories
include_directories(include)
include_directories(src/${RUN_ARCH})
include_directories(src/${RUN_ARCH}/drivers)
include_directories(src/${RUN_ARCH}/safety)
include_directories(src/${RUN_ARCH}/motion_runtime)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/arch)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/common)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/kernel)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/driver/spinlock)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/driver/jenc)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/driver/rtos_cmdqu)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/driver/fast_image)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/driver/i2c)
include_directories(${CMAKE_INSTALL_INC_PREFIX}/hal/config)

# Define MILKV_DUO_SDK for conditional compilation
add_compile_definitions(MILKV_DUO_SDK)

add_library(comm STATIC ${_ALL_SOURCES})
install(TARGETS comm DESTINATION lib)
