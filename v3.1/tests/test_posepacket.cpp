/**
 * PosePacket31 Unit Tests
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../common/protocol_posepacket31.h"
#include "../common/crc16_ccitt_false.h"
#include "../common/limits.h"
#include "../common/versioning.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s: ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL - %s\n", msg); tests_failed++; } while(0)

void test_struct_size() {
    TEST("Struct size is 42 bytes");

    if (sizeof(PosePacket31) == 42) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 42, got %zu", sizeof(PosePacket31));
        FAIL(buf);
    }
}

void test_init_sets_defaults() {
    TEST("Init sets correct defaults");

    PosePacket31 pkt;
    posepacket31_init(&pkt, 42);

    bool ok = true;
    ok = ok && (pkt.magic == SPIDER_MAGIC);
    ok = ok && (pkt.ver_major == 3);
    ok = ok && (pkt.ver_minor == 1);
    ok = ok && (pkt.seq == 42);
    ok = ok && (pkt.flags == FLAG_CLAMP_ENABLE);

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        ok = ok && (pkt.servo_us[i] == SERVO_PWM_NEUTRAL_US);
    }

    if (ok) {
        PASS();
    } else {
        FAIL("One or more fields incorrect");
    }
}

void test_magic_value() {
    TEST("Magic value is 0xB31A");

    if (SPIDER_MAGIC == 0xB31A) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 0xB31A, got 0x%04X", SPIDER_MAGIC);
        FAIL(buf);
    }
}

void test_flags_bitfield() {
    TEST("Flags bitfield values");

    bool ok = true;
    ok = ok && (FLAG_ESTOP == 0x01);
    ok = ok && (FLAG_HOLD == 0x02);
    ok = ok && (FLAG_CLAMP_ENABLE == 0x04);
    ok = ok && (FLAG_INTERP_Q16 == 0x08);
    ok = ok && (FLAG_SCAN_ENABLE == 0x20);

    if (ok) {
        PASS();
    } else {
        FAIL("Flags values incorrect");
    }
}

void test_servo_channel_count() {
    TEST("13 servo channels");

    if (SERVO_COUNT_TOTAL == 13) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 13, got %d", SERVO_COUNT_TOTAL);
        FAIL(buf);
    }
}

void test_byte_layout() {
    TEST("Byte layout matches spec");

    PosePacket31 pkt;
    memset(&pkt, 0, sizeof(pkt));

    uint8_t *bytes = (uint8_t *)&pkt;

    pkt.magic = 0xB31A;
    bool magic_ok = (bytes[0] == 0x1A && bytes[1] == 0xB3);

    pkt.ver_major = 3;
    pkt.ver_minor = 1;
    bool ver_ok = (bytes[2] == 3 && bytes[3] == 1);

    pkt.seq = 0x12345678;
    bool seq_ok = (bytes[4] == 0x78 && bytes[5] == 0x56 &&
                   bytes[6] == 0x34 && bytes[7] == 0x12);

    if (magic_ok && ver_ok && seq_ok) {
        PASS();
    } else {
        FAIL("Little-endian layout incorrect");
    }
}

int main() {
    printf("=== PosePacket31 Tests ===\n");

    test_struct_size();
    test_init_sets_defaults();
    test_magic_value();
    test_flags_bitfield();
    test_servo_channel_count();
    test_byte_layout();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
