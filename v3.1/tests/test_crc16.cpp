/**
 * CRC-16/CCITT-FALSE Unit Tests
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../common/crc16_ccitt_false.h"
#include "../common/protocol_posepacket31.h"
#include "../common/limits.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s: ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL - %s\n", msg); tests_failed++; } while(0)

void test_known_vector() {
    TEST("Known vector '123456789'");

    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_ccitt_false(data, 9);

    if (crc == 0x29B1) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 0x29B1, got 0x%04X", crc);
        FAIL(buf);
    }
}

void test_empty_data() {
    TEST("Empty data");

    uint16_t crc = crc16_ccitt_false(nullptr, 0);

    if (crc == 0xFFFF) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected 0xFFFF, got 0x%04X", crc);
        FAIL(buf);
    }
}

void test_posepacket_neutral() {
    TEST("PosePacket31 all neutral (documented test vector)");

    PosePacket31 pkt;
    posepacket31_init(&pkt, 1);
    pkt.t_ms = 100;
    pkt.flags = FLAG_CLAMP_ENABLE;

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt.servo_us[i] = 1500;
    }

    uint16_t crc = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);

    printf("(CRC=0x%04X) ", crc);
    PASS();
}

void test_crc_detects_bit_flip() {
    TEST("CRC detects single bit flip");

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t crc1 = crc16_ccitt_false(data, 5);

    data[2] ^= 0x01;
    uint16_t crc2 = crc16_ccitt_false(data, 5);

    if (crc1 != crc2) {
        PASS();
    } else {
        FAIL("CRC should differ after bit flip");
    }
}

int main() {
    printf("=== CRC-16/CCITT-FALSE Tests ===\n");

    test_known_vector();
    test_empty_data();
    test_posepacket_neutral();
    test_crc_detects_bit_flip();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
