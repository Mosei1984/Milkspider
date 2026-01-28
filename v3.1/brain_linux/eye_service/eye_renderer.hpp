#ifndef EYE_RENDERER_HPP
#define EYE_RENDERER_HPP

#include "gc9d01_dualeye_spi.hpp"
#include <cstdint>

class EyeRenderer {
public:
    using Eye = GC9D01DualEyeSpi::Eye;

    enum class Mood {
        NORMAL,
        ANGRY,
        HAPPY,
        SLEEPY,
        BLINK
    };

    explicit EyeRenderer(GC9D01DualEyeSpi &display);

    void setEyePosition(Eye eye, float x, float y);
    void setEyePosition(float x, float y);
    void setMood(Mood mood);
    void setBlink(Eye eye, float amount);
    void setIrisColor(uint16_t color);
    void render();

private:
    void renderEye(Eye eye);
    void drawFilledCircle(uint16_t *buffer, int cx, int cy, int r, uint16_t color);
    void drawCircle(uint16_t *buffer, int cx, int cy, int r, uint16_t color);
    void drawFilledRect(uint16_t *buffer, int x, int y, int w, int h, uint16_t color);
    void drawUpperLid(uint16_t *buffer, int height, bool angry, bool isLeft);
    void drawLowerLid(uint16_t *buffer, int height, bool happy);
    void drawAngryEyebrow(uint16_t *buffer, bool isLeft);

    GC9D01DualEyeSpi &m_display;

    static constexpr int WIDTH = GC9D01DualEyeSpi::WIDTH;
    static constexpr int HEIGHT = GC9D01DualEyeSpi::HEIGHT;

    float m_pos_x_left = 0.0f;
    float m_pos_y_left = 0.0f;
    float m_pos_x_right = 0.0f;
    float m_pos_y_right = 0.0f;
    float m_blink_left = 0.0f;
    float m_blink_right = 0.0f;
    Mood m_mood = Mood::NORMAL;
    uint16_t m_iris_color = 0x001F;

    uint16_t m_fb_left[GC9D01DualEyeSpi::PIXELS];
    uint16_t m_fb_right[GC9D01DualEyeSpi::PIXELS];
};

#endif // EYE_RENDERER_HPP
