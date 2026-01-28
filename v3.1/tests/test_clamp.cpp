/**
 * Servo Clamp Unit Tests
 */

#include <cstdio>
#include <cstdint>
#include "../common/limits.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s: ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL - %s\n", msg); tests_failed++; } while(0)

void test_clamp_within_range() {
    TEST("Value within range unchanged");

    uint16_t result = clamp_servo_us(1500);
    if (result == 1500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 1500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_at_min() {
    TEST("Value at min unchanged");

    uint16_t result = clamp_servo_us(500);
    if (result == 500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_at_max() {
    TEST("Value at max unchanged");

    uint16_t result = clamp_servo_us(2500);
    if (result == 2500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 2500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_below_min() {
    TEST("Value below min clamped to 500");

    uint16_t result = clamp_servo_us(400);
    if (result == 500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_above_max() {
    TEST("Value above max clamped to 2500");

    uint16_t result = clamp_servo_us(2600);
    if (result == 2500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 2500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_zero() {
    TEST("Zero clamped to 500");

    uint16_t result = clamp_servo_us(0);
    if (result == 500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 500, got %u", result);
        FAIL(buf);
    }
}

void test_clamp_max_uint16() {
    TEST("Max uint16 clamped to 2500");

    uint16_t result = clamp_servo_us(65535);
    if (result == 2500) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 2500, got %u", result);
        FAIL(buf);
    }
}

void test_limits_constants() {
    TEST("Limits constants correct");

    bool ok = true;
    ok = ok && (SERVO_PWM_MIN_US == 500);
    ok = ok && (SERVO_PWM_MAX_US == 2500);
    ok = ok && (SERVO_PWM_NEUTRAL_US == 1500);
    ok = ok && (SERVO_ANGLE_MIN_DEG == 25);
    ok = ok && (SERVO_ANGLE_MAX_DEG == 155);

    if (ok) {
        PASS();
    } else {
        FAIL("Constants mismatch");
    }
}

int main() {
    printf("=== Servo Clamp Tests ===\n");

    test_clamp_within_range();
    test_clamp_at_min();
    test_clamp_at_max();
    test_clamp_below_min();
    test_clamp_above_max();
    test_clamp_zero();
    test_clamp_max_uint16();
    test_limits_constants();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
