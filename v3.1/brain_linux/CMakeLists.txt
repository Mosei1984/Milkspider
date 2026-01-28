# Brain Linux components for Milk-V Duo 256M
#
# ARCHITECTURE NOTE (Master Prompt §1):
# The canonical Brain implementation is in src/ (NOT brain_daemon/).
# brain_daemon/ contains an earlier skeleton that was superseded.
# Build src/ directly using its own CMakeLists.txt and toolchain file.
#
# Build commands:
#   cd src && mkdir build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-milkv-duo.cmake
#   make -j4

add_subdirectory(eye_service)

# NOTE: brain_daemon/ is archived and should be removed or merged into src/
# add_subdirectory(brain_daemon)  # DISABLED - use src/ instead
