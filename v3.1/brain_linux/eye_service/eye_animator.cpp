/**
 * EyeAnimator Implementation
 */

#include "eye_animator.hpp"
#include <cstdlib>
#include <cmath>

// Timing (in ticks at 30 Hz)
#define BLINK_CLOSE_TICKS   3   // ~100ms
#define BLINK_HOLD_TICKS    2   // ~66ms
#define BLINK_OPEN_TICKS    3   // ~100ms
#define IDLE_BLINK_MIN      90  // ~3 seconds
#define IDLE_BLINK_MAX      180 // ~6 seconds
#define IDLE_LOOK_MIN       60  // ~2 seconds
#define IDLE_LOOK_MAX       150 // ~5 seconds

EyeAnimator::EyeAnimator(EyeRenderer &renderer)
    : m_renderer(renderer) {
    m_next_blink_time = IDLE_BLINK_MIN + rand() % (IDLE_BLINK_MAX - IDLE_BLINK_MIN);
    m_next_look_time = IDLE_LOOK_MIN + rand() % (IDLE_LOOK_MAX - IDLE_LOOK_MIN);
}

void EyeAnimator::tick() {
    m_tick_count++;

    updateBlink();

    if (m_idle_enabled) {
        updateIdle();
    }

    // Smooth interpolation towards target look position
    float lerp_speed = 0.1f;
    m_current_x += (m_target_x - m_current_x) * lerp_speed;
    m_current_y += (m_target_y - m_current_y) * lerp_speed;
    m_renderer.setEyePosition(m_current_x, m_current_y);

    // Render
    m_renderer.render();
}

void EyeAnimator::blink() {
    m_blink_state = BlinkState::CLOSING;
    m_blink_timer = 0;
    m_is_wink = false;
}

void EyeAnimator::wink(GC9D01DualEyeSpi::Eye eye) {
    m_blink_state = BlinkState::CLOSING;
    m_blink_timer = 0;
    m_is_wink = true;
    m_wink_eye = eye;
}

void EyeAnimator::lookAt(float x, float y) {
    m_target_x = x;
    m_target_y = y;
}

void EyeAnimator::setMood(EyeRenderer::Mood mood) {
    m_renderer.setMood(mood);
}

void EyeAnimator::updateBlink() {
    switch (m_blink_state) {
        case BlinkState::OPEN:
            // Nothing to do
            break;

        case BlinkState::CLOSING:
            m_blink_timer++;
            {
                float openness = 1.0f - (float)m_blink_timer / BLINK_CLOSE_TICKS;
                float blink_amount = 1.0f - openness;  // setBlink: 0=open, 1=closed
                if (m_is_wink) {
                    m_renderer.setBlink(m_wink_eye, blink_amount);
                } else {
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, blink_amount);
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, blink_amount);
                }
            }
            if (m_blink_timer >= BLINK_CLOSE_TICKS) {
                m_blink_state = BlinkState::CLOSED;
                m_blink_timer = 0;
            }
            break;

        case BlinkState::CLOSED:
            m_blink_timer++;
            if (m_blink_timer >= BLINK_HOLD_TICKS) {
                m_blink_state = BlinkState::OPENING;
                m_blink_timer = 0;
            }
            break;

        case BlinkState::OPENING:
            m_blink_timer++;
            {
                float openness = (float)m_blink_timer / BLINK_OPEN_TICKS;
                float blink_amount = 1.0f - openness;  // setBlink: 0=open, 1=closed
                if (m_is_wink) {
                    m_renderer.setBlink(m_wink_eye, blink_amount);
                } else {
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, blink_amount);
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, blink_amount);
                }
            }
            if (m_blink_timer >= BLINK_OPEN_TICKS) {
                m_blink_state = BlinkState::OPEN;
                if (m_is_wink) {
                    m_renderer.setBlink(m_wink_eye, 0.0f);  // 0 = open
                } else {
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::LEFT, 0.0f);
                    m_renderer.setBlink(GC9D01DualEyeSpi::Eye::RIGHT, 0.0f);
                }
            }
            break;
    }
}

void EyeAnimator::updateIdle() {
    m_idle_timer++;

    // Random blink
    if (m_idle_timer >= m_next_blink_time && m_blink_state == BlinkState::OPEN) {
        blink();
        m_next_blink_time = m_idle_timer + IDLE_BLINK_MIN +
                            rand() % (IDLE_BLINK_MAX - IDLE_BLINK_MIN);
    }

    // Random look direction
    if (m_idle_timer >= m_next_look_time) {
        m_target_x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        m_target_y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        // Keep within reasonable range
        m_target_x *= 0.5f;
        m_target_y *= 0.3f;
        m_next_look_time = m_idle_timer + IDLE_LOOK_MIN +
                           rand() % (IDLE_LOOK_MAX - IDLE_LOOK_MIN);
    }
}
