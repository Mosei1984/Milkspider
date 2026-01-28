# Toolchain file for Milk-V Duo 256M (CV1812CP, RISC-V C906)
# Cross-compilation using Milk-V SDK

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# Toolchain paths - adjust TOOLCHAIN_PATH to your SDK installation
set(TOOLCHAIN_PATH "$ENV{MILKV_SDK_PATH}" CACHE PATH "Path to Milk-V SDK toolchain")
if(NOT TOOLCHAIN_PATH OR TOOLCHAIN_PATH STREQUAL "")
    set(TOOLCHAIN_PATH "/opt/milkv/duo-sdk/gcc/riscv64-linux-musl-x86_64")
endif()

set(TOOLCHAIN_PREFIX "riscv64-unknown-linux-musl-")

# Compiler settings
set(CMAKE_C_COMPILER   "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}g++")
set(CMAKE_AR           "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}ar")
set(CMAKE_RANLIB       "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}ranlib")
set(CMAKE_STRIP        "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}strip")

# Sysroot
set(CMAKE_SYSROOT "${TOOLCHAIN_PATH}/sysroot")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# RISC-V specific flags for C906 core
set(RISCV_FLAGS "-march=rv64gc -mabi=lp64d -mcpu=c906fdv")
set(CMAKE_C_FLAGS_INIT "${RISCV_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${RISCV_FLAGS}")

# Static linking for musl libc portability
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
