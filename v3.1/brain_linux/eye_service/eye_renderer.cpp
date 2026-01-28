#include "eye_renderer.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

// Colors (RGB565)
static constexpr uint16_t COLOR_BACKGROUND = 0xFFFF;  // White
static constexpr uint16_t COLOR_SCLERA     = 0xF79E;  // Light gray
static constexpr uint16_t COLOR_PUPIL      = 0x0000;  // Black
static constexpr uint16_t COLOR_LID        = 0x0000;  // Black
static constexpr uint16_t COLOR_IRIS_DEFAULT = 0x001F; // Blue

// Eye geometry
static constexpr int EYE_CENTER_X    = 80;
static constexpr int EYE_CENTER_Y    = 80;
static constexpr int SCLERA_RADIUS   = 70;
static constexpr int IRIS_RADIUS     = 35;
static constexpr int PUPIL_RADIUS    = 15;
static constexpr int MAX_OFFSET      = 25;

EyeRenderer::EyeRenderer(GC9D01DualEyeSpi &display)
    : m_display(display), m_iris_color(COLOR_IRIS_DEFAULT) {
    std::memset(m_fb_left, 0xFF, sizeof(m_fb_left));
    std::memset(m_fb_right, 0xFF, sizeof(m_fb_right));
}

void EyeRenderer::setEyePosition(float x, float y) {
    setEyePosition(Eye::LEFT, x, y);
    setEyePosition(Eye::RIGHT, x, y);
}

void EyeRenderer::setEyePosition(Eye eye, float x, float y) {
    x = std::clamp(x, -1.0f, 1.0f);
    y = std::clamp(y, -1.0f, 1.0f);

    if (eye == Eye::LEFT) {
        m_pos_x_left = x;
        m_pos_y_left = y;
    } else {
        m_pos_x_right = x;
        m_pos_y_right = y;
    }
}

void EyeRenderer::setMood(Mood mood) {
    m_mood = mood;
}

void EyeRenderer::setBlink(Eye eye, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (eye == Eye::LEFT) {
        m_blink_left = amount;
    } else {
        m_blink_right = amount;
    }
}

void EyeRenderer::setIrisColor(uint16_t color) {
    m_iris_color = color;
}

void EyeRenderer::render() {
    renderEye(Eye::LEFT);
    renderEye(Eye::RIGHT);
    m_display.writeFramebuffer(Eye::LEFT, m_fb_left);
    m_display.writeFramebuffer(Eye::RIGHT, m_fb_right);
}

void EyeRenderer::renderEye(Eye eye) {
    uint16_t *fb = (eye == Eye::LEFT) ? m_fb_left : m_fb_right;
    float pos_x = (eye == Eye::LEFT) ? m_pos_x_left : m_pos_x_right;
    float pos_y = (eye == Eye::LEFT) ? m_pos_y_left : m_pos_y_right;
    float blink = (eye == Eye::LEFT) ? m_blink_left : m_blink_right;
    bool isLeft = (eye == Eye::LEFT);

    // Clear to background
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        fb[i] = COLOR_BACKGROUND;
    }

    // Draw sclera (white of eye)
    drawFilledCircle(fb, EYE_CENTER_X, EYE_CENTER_Y, SCLERA_RADIUS, COLOR_SCLERA);

    // Calculate iris/pupil position based on look direction
    int iris_x = EYE_CENTER_X + static_cast<int>(pos_x * MAX_OFFSET);
    int iris_y = EYE_CENTER_Y + static_cast<int>(pos_y * MAX_OFFSET);

    // Draw iris
    drawFilledCircle(fb, iris_x, iris_y, IRIS_RADIUS, m_iris_color);

    // Draw pupil
    drawFilledCircle(fb, iris_x, iris_y, PUPIL_RADIUS, COLOR_PUPIL);

    // Apply mood-specific lid shapes
    int upper_lid = 0;
    int lower_lid = 0;
    bool angry_brow = false;
    bool happy_lower = false;

    switch (m_mood) {
        case Mood::NORMAL:
            break;
        case Mood::ANGRY:
            upper_lid = 30;
            angry_brow = true;
            break;
        case Mood::HAPPY:
            lower_lid = 40;
            happy_lower = true;
            break;
        case Mood::SLEEPY:
            upper_lid = 50;
            lower_lid = 20;
            break;
        case Mood::BLINK:
            blink = 1.0f;
            break;
    }

    // Apply blink on top of mood
    if (blink > 0.0f) {
        int blink_lid = static_cast<int>(blink * (HEIGHT / 2));
        upper_lid = std::max(upper_lid, blink_lid);
        lower_lid = std::max(lower_lid, blink_lid);
    }

    // Draw upper eyelid
    if (upper_lid > 0) {
        drawUpperLid(fb, upper_lid, angry_brow, isLeft);
    }

    // Draw lower eyelid
    if (lower_lid > 0) {
        drawLowerLid(fb, lower_lid, happy_lower);
    }

    // Draw angry eyebrow on top
    if (angry_brow) {
        drawAngryEyebrow(fb, isLeft);
    }
}

void EyeRenderer::drawFilledCircle(uint16_t *buffer, int cx, int cy, int r, uint16_t color) {
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= HEIGHT) continue;

        int dx_max = static_cast<int>(std::sqrt(r2 - dy * dy));
        int x_start = std::max(0, cx - dx_max);
        int x_end = std::min(WIDTH - 1, cx + dx_max);

        for (int px = x_start; px <= x_end; px++) {
            buffer[py * WIDTH + px] = color;
        }
    }
}

void EyeRenderer::drawCircle(uint16_t *buffer, int cx, int cy, int r, uint16_t color) {
    int x = r;
    int y = 0;
    int err = 1 - r;

    auto plotPoints = [&](int px, int py) {
        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            buffer[py * WIDTH + px] = color;
        }
    };

    while (x >= y) {
        plotPoints(cx + x, cy + y);
        plotPoints(cx + y, cy + x);
        plotPoints(cx - y, cy + x);
        plotPoints(cx - x, cy + y);
        plotPoints(cx - x, cy - y);
        plotPoints(cx - y, cy - x);
        plotPoints(cx + y, cy - x);
        plotPoints(cx + x, cy - y);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void EyeRenderer::drawFilledRect(uint16_t *buffer, int x, int y, int w, int h, uint16_t color) {
    int x_start = std::max(0, x);
    int y_start = std::max(0, y);
    int x_end = std::min(WIDTH, x + w);
    int y_end = std::min(HEIGHT, y + h);

    for (int py = y_start; py < y_end; py++) {
        for (int px = x_start; px < x_end; px++) {
            buffer[py * WIDTH + px] = color;
        }
    }
}

void EyeRenderer::drawUpperLid(uint16_t *buffer, int height, bool angry, bool isLeft) {
    if (angry) {
        // Diagonal lid for angry expression
        for (int py = 0; py < height + 30; py++) {
            for (int px = 0; px < WIDTH; px++) {
                // Slope: higher on outer edge, lower on inner
                int threshold;
                if (isLeft) {
                    threshold = height - (px * 20 / WIDTH);
                } else {
                    threshold = height - ((WIDTH - px) * 20 / WIDTH);
                }
                if (py < threshold && py < HEIGHT) {
                    buffer[py * WIDTH + px] = COLOR_LID;
                }
            }
        }
    } else {
        // Straight horizontal lid
        drawFilledRect(buffer, 0, 0, WIDTH, height, COLOR_LID);
    }
}

void EyeRenderer::drawLowerLid(uint16_t *buffer, int height, bool happy) {
    if (happy) {
        // Curved lower lid (smile shape)
        for (int py = HEIGHT - height; py < HEIGHT; py++) {
            for (int px = 0; px < WIDTH; px++) {
                // Parabola: y = a*(x-center)^2 + base
                float dx = px - (WIDTH / 2.0f);
                float curve = (dx * dx) / (WIDTH * 2.0f);
                int threshold = HEIGHT - height + static_cast<int>(curve);
                if (py >= threshold) {
                    buffer[py * WIDTH + px] = COLOR_LID;
                }
            }
        }
    } else {
        // Straight horizontal lid
        drawFilledRect(buffer, 0, HEIGHT - height, WIDTH, height, COLOR_LID);
    }
}

void EyeRenderer::drawAngryEyebrow(uint16_t *buffer, bool isLeft) {
    // Draw thick diagonal eyebrow line
    int thickness = 8;
    for (int t = 0; t < thickness; t++) {
        for (int px = 0; px < WIDTH; px++) {
            int py;
            if (isLeft) {
                // Left eye: brow slopes down toward center (right side)
                py = 10 + t + (px * 25 / WIDTH);
            } else {
                // Right eye: brow slopes down toward center (left side)
                py = 10 + t + ((WIDTH - px) * 25 / WIDTH);
            }
            if (py >= 0 && py < HEIGHT && px >= 0 && px < WIDTH) {
                buffer[py * WIDTH + px] = COLOR_LID;
            }
        }
    }
}
