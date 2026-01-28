/**
 * Interpolator Unit Tests
 */

#include <cstdio>
#include <cstdint>
#include <cmath>

extern "C" {
#include "../muscle_rtos/motion_runtime/interpolator.h"
}

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s: ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL - %s\n", msg); tests_failed++; } while(0)

void test_interpolation_start_end() {
    TEST("Interpolation reaches target");

    uint16_t start[SERVO_COUNT_TOTAL];
    uint16_t target[SERVO_COUNT_TOTAL];
    uint16_t output[SERVO_COUNT_TOTAL];

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        start[i] = 1000;
        target[i] = 2000;
        output[i] = 1000;
    }

    interpolator_start(start, target, 100, INTERP_MODE_FLOAT);

    bool done = false;
    for (int tick = 0; tick < 20 && !done; tick++) {
        done = interpolator_tick(output);
    }

    bool ok = true;
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        if (output[i] != 2000) {
            ok = false;
            break;
        }
    }

    if (ok && done) {
        PASS();
    } else {
        FAIL("Did not reach target");
    }
}

void test_interpolation_midpoint() {
    TEST("Interpolation midpoint correct");

    uint16_t start[SERVO_COUNT_TOTAL];
    uint16_t target[SERVO_COUNT_TOTAL];
    uint16_t output[SERVO_COUNT_TOTAL];

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        start[i] = 1000;
        target[i] = 2000;
        output[i] = 1000;
    }

    interpolator_start(start, target, 100, INTERP_MODE_FLOAT);

    interpolator_tick(output);
    interpolator_tick(output);

    int expected_approx = 1400;
    int tolerance = 100;

    bool ok = true;
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        if (abs((int)output[i] - expected_approx) > tolerance) {
            ok = false;
            printf("\n    CH%d: expected ~%d, got %u", i, expected_approx, output[i]);
        }
    }

    if (ok) {
        PASS();
    } else {
        printf("\n");
        FAIL("Midpoint out of tolerance");
    }
}

void test_interpolation_q16() {
    TEST("Q16 interpolation works");

    uint16_t start[SERVO_COUNT_TOTAL];
    uint16_t target[SERVO_COUNT_TOTAL];
    uint16_t output[SERVO_COUNT_TOTAL];

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        start[i] = 1500;
        target[i] = 1800;
        output[i] = 1500;
    }

    interpolator_start(start, target, 60, INTERP_MODE_Q16);

    bool done = false;
    for (int tick = 0; tick < 10 && !done; tick++) {
        done = interpolator_tick(output);
    }

    bool ok = true;
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        if (output[i] != 1800) {
            ok = false;
            break;
        }
    }

    if (ok) {
        PASS();
    } else {
        FAIL("Q16 did not reach target");
    }
}

void test_interpolation_abort() {
    TEST("Abort stops interpolation");

    uint16_t start[SERVO_COUNT_TOTAL] = {1500};
    uint16_t target[SERVO_COUNT_TOTAL] = {2000};
    uint16_t output[SERVO_COUNT_TOTAL] = {1500};

    interpolator_start(start, target, 1000, INTERP_MODE_FLOAT);

    interpolator_tick(output);
    interpolator_abort();

    bool done = interpolator_tick(output);

    if (done) {
        PASS();
    } else {
        FAIL("Abort did not stop interpolation");
    }
}

int main() {
    printf("=== Interpolator Tests ===\n");

    test_interpolation_start_end();
    test_interpolation_midpoint();
    test_interpolation_q16();
    test_interpolation_abort();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
