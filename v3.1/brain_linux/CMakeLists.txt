# Brain Linux components for Milk-V Duo 256M
#
# ARCHITECTURE NOTE (Master Prompt §1):
# - brain_linux/src/     = CANONICAL Brain Daemon (WebSocket :9000, Serial, IPC)
# - brain_linux/eye_service/ = Eye animation service (SPI displays)
# - brain_linux/archive/brain_daemon_skeleton/ = ARCHIVED, DO NOT USE
#
# Both components are built by this CMakeLists.txt when invoked from root.

add_subdirectory(src)
add_subdirectory(eye_service)
