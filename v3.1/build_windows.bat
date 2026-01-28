@echo off
REM Spider Robot v3.1 - Windows Cross-Compilation Script
REM Requires: WSL2 with Milk-V SDK or Docker

echo === Spider v3.1 Cross-Compiler ===
echo.

REM Check for WSL
where wsl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: WSL not found. Install WSL2 for cross-compilation.
    echo See: https://learn.microsoft.com/en-us/windows/wsl/install
    exit /b 1
)

REM Set SDK path (adjust if different)
set MILKV_SDK_PATH=/opt/milkv/duo-sdk/riscv64-linux-musl-x86_64

REM Create build directory
if not exist build mkdir build
cd build

echo [1/3] Configuring CMake via WSL...
wsl cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-milkv-duo.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_TESTS=OFF

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo [2/3] Building...
wsl cmake --build . --parallel

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [3/3] Build complete!
echo.
echo Binaries:
dir /b brain_linux\brain_daemon\brain_daemon 2>nul
dir /b brain_linux\eye_service\eye_service 2>nul
echo.
echo Deploy to Milk-V Duo:
echo   scp brain_linux/brain_daemon/brain_daemon root@milkv-duo:/usr/local/bin/
echo   scp brain_linux/eye_service/eye_service root@milkv-duo:/usr/local/bin/
