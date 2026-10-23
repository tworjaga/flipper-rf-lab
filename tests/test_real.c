// Real-source unit tests.
//
// Unlike the legacy reimplementations, this file links against the ACTUAL
// production sources (core/math/fixed_point.c, core/math/statistics.c,
// storage/compression.c) through a minimal host shim (tests/hostshim), so the
// tests exercise the code that ships, not a copy of it.
//
// Build + run (from the repo root), with a host C compiler:
//   MSVC:  see tests/run_host_tests.cmd
//   gcc:   gcc -I tests/hostshim -I core -o test_real tests/test_real.c \
//                core/math/fixed_point.c core/math/statistics.c \
//                storage/compression.c && ./test_real
//
// Many cases are regressions for specific bugs fixed in review (noted inline).

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../core/math/fixed_point.h"
#include "../storage/compression.h"   // pulls Pulse_t/Frame_t via the shim

static int g_pass = 0, g_fail = 0;

static void check(const char* name, int cond) {
    if(cond) { g_pass++; printf("  [PASS] %s\n", name); }
    else     { g_fail++; printf("  [FAIL] %s\n", name); }
}

// --- fixed-point core ------------------------------------------------------
static void test_fixed_point(void) {
    printf("Fixed-point (real fixed_point.c)\n");
    fixed_t a = INT_TO_FIXED(3), b = INT_TO_FIXED(4);
    check("3*4 == 12", FIXED_TO_INT(fixed_mul(a, b)) == 12);
    check("12/4 == 3", FIXED_TO_INT(fixed_div(fixed_mul(a, b), b)) == 3);
    check("sqrt(16) == 4", FIXED_TO_INT(fixed_sqrt(INT_TO_FIXED(16))) == 4);
    check("div by zero saturates", fixed_div(INT_TO_FIXED(1), 0) == FIXED_MAX);
}

// --- delta codec (regressions for the -128/-127 escape-collision fix) ------
static void test_delta(void) {
    printf("Delta 8-bit round-trip (real compression.c)\n");
    // Includes consecutive values that force -128 and -127 deltas, the exact
    // cases that used to collide with the 0x80/0x81 escape markers.
    uint8_t in[] = {100, 228, 100, 228, 101, 0, 255, 0};
    uint8_t enc[64], dec[64];
    uint32_t n = delta_encode(in, sizeof(in), enc, sizeof(enc));
    uint32_t m = delta_decode(enc, n, dec, sizeof(dec));
    check("delta: length preserved", m == sizeof(in));
    check("delta: round-trips exactly", m == sizeof(in) && memcmp(in, dec, m) == 0);

    uint8_t small[8]; for(int i = 0; i < 8; i++) small[i] = (uint8_t)(i + 1);
    check("delta: encoded <= raw for ramp", delta_encode(small, 8, enc, sizeof(enc)) <= 8);
}

static void test_delta16(void) {
    printf("Delta 16-bit round-trip (real compression.c)\n");
    // Big jumps exercise the 16-bit and 32-bit escape paths + the -127 case.
    uint16_t in[] = {1000, 1127, 1000, 40000, 5, 65535, 0};
    uint8_t enc[128]; uint16_t dec[16];
    uint32_t n = delta_encode_16bit(in, 7, enc, sizeof(enc));
    uint32_t m = delta_decode_16bit(enc, n, dec, 16);
    check("delta16: count preserved", m == 7);
    check("delta16: round-trips exactly", m == 7 && memcmp(in, dec, 7 * sizeof(uint16_t)) == 0);
}

// --- RLE / LZ77 round-trips ------------------------------------------------
static void test_rle(void) {
    printf("RLE round-trip (real compression.c)\n");
    uint8_t in[] = {0,0,0,0,0, 7, 9,9,9, 0, 0, 3};
    uint8_t enc[64], dec[64];
    uint32_t n = rle_encode(in, sizeof(in), enc, sizeof(enc));
    uint32_t m = rle_decode(enc, n, dec, sizeof(dec));
    check("rle: round-trips exactly", m == sizeof(in) && memcmp(in, dec, m) == 0);
}

static void test_lz77(void) {
    printf("LZ77 round-trip (real compression.c)\n");
    uint8_t in[] = "ABCABCABCABCDEF_ABCABC_0000000";
    uint32_t len = (uint32_t)strlen((char*)in);
    uint8_t enc[128], dec[128];
    uint32_t n = lz77_encode(in, len, enc, sizeof(enc), 4096, 18);
    uint32_t m = lz77_decode(enc, n, dec, sizeof(dec));
    check("lz77: round-trips exactly", m == len && memcmp(in, dec, m) == 0);
}

// --- pulse-sequence codec (regression for the width-section length fix) -----
static void test_pulses(void) {
    printf("Pulse sequence round-trip (real compression.c)\n");
    Pulse_t pin[20], pout[20];
    memset(pin, 0, sizeof(pin));
    for(int i = 0; i < 20; i++) {
        pin[i].width_us = (uint16_t)(200 + (i % 3) * 400);  // 200/600/1000
        pin[i].level    = (uint8_t)(i & 1);
    }
    uint8_t buf[256];
    uint32_t n = compress_pulse_sequence(pin, 20, buf, sizeof(buf));
    uint32_t got = decompress_pulse_sequence(buf, n, pout, 20);
    int ok = (got == 20);
    for(uint32_t i = 0; ok && i < got; i++) {
        if(pout[i].width_us != pin[i].width_us || pout[i].level != pin[i].level) ok = 0;
    }
    check("pulses: count + widths + levels round-trip", ok);
}

int main(void) {
    printf("==== Flipper RF Lab — real-source unit tests ====\n");
    compression_init();
    test_fixed_point();
    test_delta();
    test_delta16();
    test_rle();
    test_lz77();
    test_pulses();
    printf("=================================================\n");
    printf("Passed: %d   Failed: %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
