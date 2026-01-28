#ifndef EYE_ANIMATOR_HPP
#define EYE_ANIMATOR_HPP

#include "eye_renderer.hpp"
#include <cstdint>

/**
 * EyeAnimator - Manages eye animations
 *
 * Features:
 * - Blink animation
 * - Wink animation
 * - Look (follow direction)
 * - Idle random movement
 */
class EyeAnimator {
public:
    explicit EyeAnimator(EyeRenderer &renderer);

    /**
     * Tick the animator (call at ~30 Hz).
     */
    void tick();

    /**
     * Trigger a blink on both eyes.
     */
    void blink();

    /**
     * Trigger a wink on one eye.
     */
    void wink(GC9D01DualEyeSpi::Eye eye);

    /**
     * Set look direction (immediate).
     */
    void lookAt(float x, float y);

    /**
     * Set mood.
     */
    void setMood(EyeRenderer::Mood mood);

    /**
     * Enable/disable idle animation.
     */
    void setIdleEnabled(bool enabled) { m_idle_enabled = enabled; }

private:
    void updateBlink();
    void updateIdle();

    EyeRenderer &m_renderer;

    // Blink state
    enum class BlinkState {
        OPEN,
        CLOSING,
        CLOSED,
        OPENING
    };
    BlinkState m_blink_state = BlinkState::OPEN;
    uint32_t m_blink_timer = 0;
    GC9D01DualEyeSpi::Eye m_wink_eye;
    bool m_is_wink = false;

    // Idle state
    bool m_idle_enabled = true;
    uint32_t m_idle_timer = 0;
    uint32_t m_next_blink_time = 0;
    uint32_t m_next_look_time = 0;
    float m_target_x = 0.0f;
    float m_target_y = 0.0f;
    float m_current_x = 0.0f;
    float m_current_y = 0.0f;

    uint32_t m_tick_count = 0;
};

#endif // EYE_ANIMATOR_HPP
