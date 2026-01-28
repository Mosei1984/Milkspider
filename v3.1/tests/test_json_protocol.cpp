/**
 * JSON Protocol Unit Tests
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../brain_linux/brain_daemon/json_protocol.hpp"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s: ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL - %s\n", msg); tests_failed++; } while(0)

void test_parse_motion_command() {
    TEST("Parse motion command");

    JsonProtocol parser;
    std::string json = R"({
        "v": "3.1",
        "type": "motion",
        "msg": {
            "mode": "legacy_prg",
            "cmd": "forward",
            "vec": {"fwd": 0.8, "strafe": 0.0, "turn": 0.1},
            "stride": 1.2,
            "speed": 0.9
        }
    })";

    auto result = parser.parseMessage(json);

    if (!result.valid) {
        FAIL("Parse failed");
        return;
    }

    auto motion = parser.getMotionMsg();
    if (!motion) {
        FAIL("No motion msg");
        return;
    }

    bool ok = true;
    ok = ok && (motion->mode == "legacy_prg");
    ok = ok && (motion->cmd == "forward");
    ok = ok && (motion->vec_fwd > 0.7f && motion->vec_fwd < 0.9f);
    ok = ok && (motion->stride > 1.1f && motion->stride < 1.3f);

    if (ok) {
        PASS();
    } else {
        FAIL("Values incorrect");
    }
}

void test_parse_eyes_command() {
    TEST("Parse eyes command");

    JsonProtocol parser;
    std::string json = R"({
        "v": "3.1",
        "type": "eyes",
        "msg": {
            "cmd": "look",
            "L": {"x": -0.5, "y": 0.3},
            "R": {"x": 0.5, "y": 0.3}
        }
    })";

    auto result = parser.parseMessage(json);

    if (!result.valid) {
        FAIL("Parse failed");
        return;
    }

    auto eye = parser.getEyeMsg();
    if (!eye) {
        FAIL("No eye msg");
        return;
    }

    bool ok = true;
    ok = ok && (eye->cmd == "look");
    ok = ok && (eye->lx < -0.4f && eye->lx > -0.6f);
    ok = ok && (eye->rx > 0.4f && eye->rx < 0.6f);

    if (ok) {
        PASS();
    } else {
        FAIL("Values incorrect");
    }
}

void test_parse_system_command() {
    TEST("Parse system command");

    JsonProtocol parser;
    std::string json = R"({
        "v": "3.1",
        "type": "sys",
        "msg": {
            "cmd": "boot_demo",
            "wakepose": "combat"
        }
    })";

    auto result = parser.parseMessage(json);

    if (!result.valid) {
        FAIL("Parse failed");
        return;
    }

    auto sys = parser.getSystemMsg();
    if (!sys) {
        FAIL("No system msg");
        return;
    }

    bool ok = (sys->cmd == "boot_demo") && (sys->wakepose == "combat");

    if (ok) {
        PASS();
    } else {
        FAIL("Values incorrect");
    }
}

void test_reject_wrong_version() {
    TEST("Reject wrong version");

    JsonProtocol parser;
    std::string json = R"({
        "v": "2.0",
        "type": "motion",
        "msg": {}
    })";

    auto result = parser.parseMessage(json);

    if (!result.valid) {
        PASS();
    } else {
        FAIL("Should have rejected version 2.0");
    }
}

void test_clamp_values() {
    TEST("Clamp out-of-range values");

    JsonProtocol parser;
    std::string json = R"({
        "v": "3.1",
        "type": "motion",
        "msg": {
            "vec": {"fwd": 5.0, "strafe": -3.0, "turn": 0.5},
            "stride": 10.0,
            "speed": -0.5
        }
    })";

    auto result = parser.parseMessage(json);
    auto motion = parser.getMotionMsg();

    if (!motion) {
        FAIL("No motion msg");
        return;
    }

    bool ok = true;
    ok = ok && (motion->vec_fwd == 1.0f);
    ok = ok && (motion->vec_strafe == -1.0f);
    ok = ok && (motion->stride == 1.6f);
    ok = ok && (motion->speed == 0.0f);

    if (ok) {
        PASS();
    } else {
        printf("\n    fwd=%f strafe=%f stride=%f speed=%f\n",
               motion->vec_fwd, motion->vec_strafe, motion->stride, motion->speed);
        FAIL("Clamping incorrect");
    }
}

void test_create_telemetry() {
    TEST("Create telemetry response");

    std::string json = JsonProtocol::createTelemetryResponse(120, 998.5f, 5000, "running");

    bool ok = true;
    ok = ok && (json.find("\"v\":\"3.1\"") != std::string::npos);
    ok = ok && (json.find("\"type\":\"telemetry\"") != std::string::npos);
    ok = ok && (json.find("\"uptime_s\":120") != std::string::npos);

    if (ok) {
        PASS();
    } else {
        FAIL("Response format incorrect");
    }
}

int main() {
    printf("=== JSON Protocol Tests ===\n");

    test_parse_motion_command();
    test_parse_eyes_command();
    test_parse_system_command();
    test_reject_wrong_version();
    test_clamp_values();
    test_create_telemetry();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
