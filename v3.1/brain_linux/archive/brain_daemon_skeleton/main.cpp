/**
 * Spider Robot v3.1 - Brain Daemon (Linux)
 *
 * Main entry point for the Brain core running on Linux.
 * Responsibilities:
 * - WebSocket server for Remote/Browser UI (JSON)
 * - Task management (cooperative, non-blocking)
 * - Error handling and safety escalation
 * - Eye Service communication (Unix socket)
 * - Motion packet generation (PosePacket31 → Muscle via RPMsg)
 * - Obstacle avoidance policy
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>

#include "task_manager.hpp"
#include "error_handler.hpp"
#include "ws_server.hpp"
#include "ipc_muscle_rpmsg.hpp"
#include "eye_client_unix.hpp"
#include "config_store.hpp"
#include "telemetry.hpp"

// Global shutdown flag
static std::atomic<bool> g_shutdown{false};

void signal_handler(int sig) {
    (void)sig;
    g_shutdown.store(true);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "[Brain] Spider Robot v3.1 Brain Daemon starting..." << std::endl;

    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Load configuration
    ConfigStore config;
    if (!config.load("/etc/spider/v3.1/config.json")) {
        std::cerr << "[Brain] Warning: Config not found, using defaults" << std::endl;
    }

    // Initialize error handler
    ErrorHandler error_handler;

    // Initialize RPMsg IPC to Muscle
    IpcMuscleRpmsg muscle_ipc;
    if (!muscle_ipc.init()) {
        error_handler.report(ErrorLevel::CRITICAL, "Failed to init RPMsg");
        return 1;
    }

    // Initialize Eye Service client
    EyeClientUnix eye_client;
    if (!eye_client.connect("/tmp/spider_eye.sock")) {
        error_handler.report(ErrorLevel::WARNING, "Eye service not available");
    }

    // Initialize telemetry
    Telemetry telemetry;

    // Initialize task manager
    TaskManager task_manager(muscle_ipc, eye_client, config, telemetry);

    // Initialize WebSocket server
    WsServer ws_server(8080, task_manager, error_handler);
    if (!ws_server.start()) {
        error_handler.report(ErrorLevel::CRITICAL, "Failed to start WebSocket server");
        return 1;
    }

    std::cout << "[Brain] All systems initialized. Running..." << std::endl;

    // Main loop (cooperative, non-blocking)
    while (!g_shutdown.load()) {
        // Process WebSocket events
        ws_server.poll();

        // Run task manager tick
        task_manager.tick();

        // Process telemetry
        telemetry.tick();

        // Yield to avoid busy-waiting (1 ms sleep)
        usleep(1000);
    }

    std::cout << "[Brain] Shutting down..." << std::endl;

    // Cleanup
    ws_server.stop();
    muscle_ipc.close();
    eye_client.disconnect();

    return 0;
}
