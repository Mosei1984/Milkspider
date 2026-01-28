/**
 * Spider Robot v3.1 - Eye Service (Linux)
 *
 * Standalone service for DualEye display control.
 * Receives events from Brain Daemon via Unix socket.
 * Renders eye animations on two GC9D01 displays via SPI.
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>

#include "gc9d01_dualeye_spi.hpp"
#include "eye_renderer.hpp"
#include "eye_animator.hpp"

extern "C" {
#include "eye_event_protocol.h"
}

#define RX_BUFFER_SIZE 512
#define BOOT_FRAME_DELAY_US 33333  // ~30 Hz

static std::atomic<bool> g_shutdown{false};

void signal_handler(int sig) {
    (void)sig;
    g_shutdown.store(true);
}

static bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static void runBootAnimation(EyeRenderer& renderer, EyeAnimator& animator) {
    std::cout << "[Eye] Running boot animation..." << std::endl;

    animator.setIdleEnabled(false);
    animator.setMood(EyeRenderer::Mood::SLEEPY);
    renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, 1.0f);
    renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, 1.0f);
    renderer.setEyePosition(0.0f, 0.0f);
    renderer.render();
    usleep(BOOT_FRAME_DELAY_US * 3);

    for (int i = 0; i <= 15 && !g_shutdown.load(); i++) {
        float openness = (float)i / 15.0f;
        renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, 1.0f - openness);
        renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, 1.0f - openness);
        renderer.render();
        usleep(BOOT_FRAME_DELAY_US);
    }

    animator.setMood(EyeRenderer::Mood::NORMAL);
    renderer.render();
    usleep(BOOT_FRAME_DELAY_US * 3);

    struct LookStep { float x; float y; int frames; };
    LookStep lookSequence[] = {
        { -0.7f,  0.0f, 8 },
        {  0.7f,  0.0f, 10 },
        {  0.0f, -0.4f, 8 },
        {  0.0f,  0.4f, 8 },
        {  0.0f,  0.0f, 6 },
    };

    float curX = 0.0f, curY = 0.0f;
    for (const auto& step : lookSequence) {
        if (g_shutdown.load()) return;
        float startX = curX, startY = curY;
        for (int f = 1; f <= step.frames && !g_shutdown.load(); f++) {
            float t = (float)f / step.frames;
            float smoothT = t * t * (3.0f - 2.0f * t);
            curX = startX + (step.x - startX) * smoothT;
            curY = startY + (step.y - startY) * smoothT;
            renderer.setEyePosition(curX, curY);
            renderer.render();
            usleep(BOOT_FRAME_DELAY_US);
        }
        usleep(BOOT_FRAME_DELAY_US * 2);
    }

    for (int blink = 0; blink < 2 && !g_shutdown.load(); blink++) {
        for (int i = 0; i <= 3 && !g_shutdown.load(); i++) {
            float amt = (float)i / 3.0f;
            renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, amt);
            renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, amt);
            renderer.render();
            usleep(BOOT_FRAME_DELAY_US);
        }
        usleep(BOOT_FRAME_DELAY_US * 2);
        for (int i = 3; i >= 0 && !g_shutdown.load(); i--) {
            float amt = (float)i / 3.0f;
            renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, amt);
            renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, amt);
            renderer.render();
            usleep(BOOT_FRAME_DELAY_US);
        }
        usleep(BOOT_FRAME_DELAY_US * 3);
    }

    animator.setMood(EyeRenderer::Mood::NORMAL);
    animator.lookAt(0.0f, 0.0f);
    animator.setIdleEnabled(true);
    renderer.render();

    std::cout << "[Eye] Boot animation complete" << std::endl;
}

static void parseAndHandleEvent(const char* json, EyeAnimator& animator, EyeRenderer& renderer) {
    if (strstr(json, "\"type\":\"mood\"")) {
        if (strstr(json, "\"mood\":\"angry\"")) {
            animator.setMood(EyeRenderer::Mood::ANGRY);
        } else if (strstr(json, "\"mood\":\"happy\"")) {
            animator.setMood(EyeRenderer::Mood::HAPPY);
        } else if (strstr(json, "\"mood\":\"sleepy\"")) {
            animator.setMood(EyeRenderer::Mood::SLEEPY);
        } else {
            animator.setMood(EyeRenderer::Mood::NORMAL);
        }
        std::cout << "[Eye] Mood changed" << std::endl;
        return;
    }

    if (strstr(json, "\"type\":\"look\"")) {
        float x = 0.0f, y = 0.0f;
        const char* px = strstr(json, "\"x\":");
        const char* py = strstr(json, "\"y\":");
        if (px) x = strtof(px + 4, nullptr);
        if (py) y = strtof(py + 4, nullptr);
        animator.lookAt(x, y);
        return;
    }

    if (strstr(json, "\"type\":\"blink\"")) {
        animator.blink();
        std::cout << "[Eye] Blink triggered" << std::endl;
        return;
    }

    if (strstr(json, "\"type\":\"wink\"")) {
        if (strstr(json, "\"eye\":\"right\"")) {
            animator.wink(GC9D01DualEyeSpi::Eye::RIGHT);
        } else {
            animator.wink(GC9D01DualEyeSpi::Eye::LEFT);
        }
        std::cout << "[Eye] Wink triggered" << std::endl;
        return;
    }

    if (strstr(json, "\"type\":\"color\"")) {
        const char* pc = strstr(json, "\"rgb565\":");
        if (pc) {
            uint16_t color = (uint16_t)strtol(pc + 9, nullptr, 0);
            renderer.setIrisColor(color);
            std::cout << "[Eye] Iris color set to 0x" << std::hex << color << std::dec << std::endl;
        }
        return;
    }

    if (strstr(json, "\"type\":\"idle\"")) {
        if (strstr(json, "\"enabled\":true")) {
            animator.setIdleEnabled(true);
            std::cout << "[Eye] Idle enabled" << std::endl;
        } else if (strstr(json, "\"enabled\":false")) {
            animator.setIdleEnabled(false);
            std::cout << "[Eye] Idle disabled" << std::endl;
        }
        return;
    }

    if (strstr(json, "\"type\":\"estop\"")) {
        animator.setMood(EyeRenderer::Mood::SLEEPY);
        animator.setIdleEnabled(false);
        std::cout << "[Eye] ESTOP - eyes sleepy" << std::endl;
        return;
    }

    if (strstr(json, "\"type\":\"status\"")) {
        std::cout << "[Eye] Status: running" << std::endl;
        return;
    }
}

static void printUsage(const char* progname) {
    std::cout << "Usage: " << progname << " [OPTIONS]\n"
              << "Options:\n"
              << "  --skip-boot    Skip boot animation\n"
              << "  --help         Show this help\n";
}

int main(int argc, char *argv[]) {
    bool skipBoot = false;

    static struct option long_options[] = {
        {"skip-boot", no_argument, 0, 's'},
        {"help",      no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "sh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 's':
                skipBoot = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    std::cout << "[Eye] Spider Robot v3.1 Eye Service starting..." << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    GC9D01DualEyeSpi display;
    if (!display.init()) {
        std::cerr << "[Eye] Failed to initialize display" << std::endl;
        return 1;
    }

    EyeRenderer renderer(display);
    EyeAnimator animator(renderer);

    if (!skipBoot) {
        runBootAnimation(renderer, animator);
    }

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "[Eye] Failed to create socket" << std::endl;
        return 1;
    }

    unlink(EYE_SOCKET_PATH);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, EYE_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Eye] Failed to bind socket" << std::endl;
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 2) < 0) {
        std::cerr << "[Eye] Failed to listen" << std::endl;
        close(server_fd);
        return 1;
    }

    setNonBlocking(server_fd);
    std::cout << "[Eye] Listening on " << EYE_SOCKET_PATH << std::endl;

    int client_fd = -1;
    char rx_buffer[RX_BUFFER_SIZE];
    size_t rx_len = 0;

    while (!g_shutdown.load()) {
        // Accept new client if none connected
        if (client_fd < 0) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (new_fd >= 0) {
                setNonBlocking(new_fd);
                client_fd = new_fd;
                rx_len = 0;
                std::cout << "[Eye] Brain connected" << std::endl;
            }
        }

        // Read and process events from client
        if (client_fd >= 0) {
            ssize_t n = recv(client_fd, rx_buffer + rx_len, sizeof(rx_buffer) - rx_len - 1, 0);
            if (n > 0) {
                rx_len += n;
                rx_buffer[rx_len] = '\0';

                // Process complete lines (newline-delimited JSON)
                char* line_start = rx_buffer;
                char* newline;
                while ((newline = strchr(line_start, '\n')) != nullptr) {
                    *newline = '\0';
                    if (strlen(line_start) > 0) {
                        parseAndHandleEvent(line_start, animator, renderer);
                    }
                    line_start = newline + 1;
                }

                // Move remaining partial line to buffer start
                size_t remaining = rx_len - (line_start - rx_buffer);
                if (remaining > 0 && line_start != rx_buffer) {
                    memmove(rx_buffer, line_start, remaining);
                }
                rx_len = remaining;
            } else if (n == 0) {
                std::cout << "[Eye] Brain disconnected" << std::endl;
                close(client_fd);
                client_fd = -1;
                rx_len = 0;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "[Eye] recv error: " << strerror(errno) << std::endl;
                close(client_fd);
                client_fd = -1;
                rx_len = 0;
            }
        }

        animator.tick();
        usleep(33333);  // ~30 Hz
    }

    std::cout << "[Eye] Shutting down..." << std::endl;

    if (client_fd >= 0) {
        close(client_fd);
    }
    close(server_fd);
    unlink(EYE_SOCKET_PATH);

    return 0;
}
