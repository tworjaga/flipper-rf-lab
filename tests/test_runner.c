// Unit Test Runner for Flipper RF Lab
// Tests portable components without Flipper SDK dependencies

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// Mock Flipper SDK types for testing
typedef int32_t FuriStatus;
#define FuriStatusOk 0
#define FuriStatusError -1
#define FURI_LOG_I(tag, fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define FURI_LOG_E(tag, fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define FURI_LOG_W(tag, fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define UNUSED(x) (void)(x)
#define furi_get_tick() (clock() * 1000 / CLOCKS_PER_SEC)

// Include components under test
#define TESTING_MODE 1

// Minimal fixed-point implementation for testing
typedef int32_t fixed_t;
#define FIXED_SCALE 65536
#define FIXED_ONE 65536
#define FIXED_TWO 131072
#define FIXED_HALF 32768
#define FIXED_PI 205887
#define FIXED_MAX 2147483647
#define FIXED_MIN -2147483648

#define INT_TO_FIXED(x) ((x) * FIXED_SCALE)
#define FIXED_TO_INT(x) ((x) / FIXED_SCALE)
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * FIXED_SCALE))
#define FIXED_TO_FLOAT(x) ((float)(x) / FIXED_SCALE)

// Test statistics
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    const char* current_suite;
} TestStats;

static TestStats stats = {0};

// Test assertion macros
#define TEST_ASSERT(condition, message) \
    do { \
        stats.total_tests++; \
        if (condition) { \
            stats.passed_tests++; \
            printf("  ✓ PASS: %s\n", message); \
        } else { \
            stats.failed_tests++; \
            printf("  ✗ FAIL: %s (line %d)\n", message, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQ_INT(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_EQ_FLOAT(expected, actual, tolerance, message) \
    TEST_ASSERT(fabs((expected) - (actual)) < (tolerance), message)

#define TEST_SUITE(name) \
    do { \
        stats.current_suite = name; \
        printf("\n=== %s ===\n", name); \
    } while(0)

// ============================================================================
// FIXED-POINT MATH TESTS
// ============================================================================

fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) / FIXED_SCALE);
}

fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return 0;
    return (fixed_t)(((int64_t)a * FIXED_SCALE) / (int64_t)b);
}

fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    fixed_t res = x;
    for (int i = 0; i < 20; i++) {
        res = (res + fixed_div(x, res)) / 2;
    }
    return res;
}

fixed_t fixed_abs(fixed_t x) {
    return (x < 0) ? -x : x;
}

void test_fixed_point_math() {
    TEST_SUITE("Fixed-Point Math");
    
    // Test basic operations
    fixed_t a = INT_TO_FIXED(10);
    fixed_t b = INT_TO_FIXED(5);
    
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(15), a + b, "Addition");
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(5), a - b, "Subtraction");
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(50), fixed_mul(a, b), "Multiplication");
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(2), fixed_div(a, b), "Division");
    
    // Test square root
    fixed_t sqrt_16 = fixed_sqrt(INT_TO_FIXED(16));
    float sqrt_float = FIXED_TO_FLOAT(sqrt_16);
    TEST_ASSERT_EQ_FLOAT(4.0f, sqrt_float, 0.1f, "Square root of 16");
    
    // Test absolute value
    fixed_t neg = INT_TO_FIXED(-10);
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(10), fixed_abs(neg), "Absolute value");
    
    // Test conversion
    float f = 3.14159f;
    fixed_t pi_fixed = FLOAT_TO_FIXED(f);
    float f_back = FIXED_TO_FLOAT(pi_fixed);
    TEST_ASSERT_EQ_FLOAT(f, f_back, 0.0001f, "Float conversion round-trip");
}

// ============================================================================
// COMPRESSION TESTS
// ============================================================================

uint32_t delta_encode(const uint8_t* input, uint32_t len, uint8_t* output) {
    if (len == 0) return 0;
    uint32_t out_pos = 0;
    int16_t last = input[0];
    output[out_pos++] = input[0];
    
    for (uint32_t i = 1; i < len && out_pos < 1024; i++) {
        int16_t delta = (int16_t)input[i] - last;
        if (delta >= -128 && delta <= 127) {
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        } else {
            output[out_pos++] = 0x80;
            output[out_pos++] = (uint8_t)(delta >> 8);
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        }
        last = input[i];
    }
    return out_pos;
}

uint32_t delta_decode(const uint8_t* input, uint32_t len, uint8_t* output) {
    if (len == 0) return 0;
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    int16_t last = input[in_pos++];
    output[out_pos++] = (uint8_t)last;
    
    while (in_pos < len) {
        uint8_t byte = input[in_pos++];
        if (byte == 0x80 && in_pos + 1 < len) {
            int16_t delta = (int16_t)((input[in_pos] << 8) | input[in_pos + 1]);
            in_pos += 2;
            last += delta;
        } else {
            last += (int8_t)byte;
        }
        output[out_pos++] = (uint8_t)last;
    }
    return out_pos;
}

uint32_t rle_encode(const uint8_t* input, uint32_t len, uint8_t* output) {
    uint32_t in_pos = 0, out_pos = 0;
    while (in_pos < len && out_pos < 1024 - 3) {
        uint8_t symbol = input[in_pos];
        uint8_t run_length = 1;
        while (in_pos + run_length < len && 
               input[in_pos + run_length] == symbol && 
               run_length < 255) {
            run_length++;
        }
        if (run_length >= 3) {
            output[out_pos++] = 0x00;
            output[out_pos++] = run_length;
            output[out_pos++] = symbol;
            in_pos += run_length;
        } else {
            if (symbol == 0x00) {
                output[out_pos++] = 0x00;
                output[out_pos++] = 0x01;
            }
            output[out_pos++] = symbol;
            in_pos++;
        }
    }
    return out_pos;
}

uint32_t rle_decode(const uint8_t* input, uint32_t len, uint8_t* output) {
    uint32_t in_pos = 0, out_pos = 0;
    while (in_pos < len) {
        uint8_t byte = input[in_pos++];
        if (byte == 0x00) {
            if (in_pos >= len) break;
            uint8_t next = input[in_pos++];
            if (next == 0x00) {
                output[out_pos++] = 0x00;
            } else if (next == 0x01) {
                if (in_pos >= len) break;
                output[out_pos++] = input[in_pos++];
            } else {
                uint8_t run_length = next;
                if (in_pos >= len) break;
                uint8_t symbol = input[in_pos++];
                for (uint8_t i = 0; i < run_length && out_pos < 1024; i++) {
                    output[out_pos++] = symbol;
                }
            }
        } else {
            output[out_pos++] = byte;
        }
    }
    return out_pos;
}

void test_compression() {
    TEST_SUITE("Compression");
    
    // Test data - sequential bytes (good for delta)
    uint8_t test_data[100];
    for (int i = 0; i < 100; i++) {
        test_data[i] = i;
    }
    
    uint8_t compressed[256];
    uint8_t decompressed[256];
    
    // Test Delta encoding
    uint32_t compressed_len = delta_encode(test_data, 100, compressed);
    uint32_t decompressed_len = delta_decode(compressed, compressed_len, decompressed);
    
    TEST_ASSERT(compressed_len < 100, "Delta compression reduces size");
    TEST_ASSERT_EQ_INT(100, decompressed_len, "Delta decompressed length matches");
    TEST_ASSERT(memcmp(test_data, decompressed, 100) == 0, "Delta round-trip successful");
    
    printf("  Delta: %d bytes -> %d bytes (%.1f%%)\n", 
           100, compressed_len, (compressed_len * 100.0) / 100.0);
    
    // Test RLE encoding with repetitive data
    uint8_t rle_data[100];
    for (int i = 0; i < 100; i++) {
        rle_data[i] = (i < 50) ? 0xAA : 0xBB;
    }
    
    uint32_t rle_compressed_len = rle_encode(rle_data, 100, compressed);
    uint32_t rle_decompressed_len = rle_decode(compressed, rle_compressed_len, decompressed);
    
    TEST_ASSERT(rle_compressed_len < 100, "RLE compression reduces size");
    TEST_ASSERT_EQ_INT(100, rle_decompressed_len, "RLE decompressed length matches");
    TEST_ASSERT(memcmp(rle_data, decompressed, 100) == 0, "RLE round-trip successful");
    
    printf("  RLE: %d bytes -> %d bytes (%.1f%%)\n", 
           100, rle_compressed_len, (rle_compressed_len * 100.0) / 100.0);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

typedef struct {
    uint32_t n;
    fixed_t mean;
    fixed_t m2;
    fixed_t min_val;
    fixed_t max_val;
} WelfordState_t;

void welford_init(WelfordState_t* state) {
    memset(state, 0, sizeof(WelfordState_t));
    state->min_val = FIXED_MAX;
    state->max_val = FIXED_MIN;
}

void welford_add_sample(WelfordState_t* state, fixed_t x) {
    state->n++;
    if (x < state->min_val) state->min_val = x;
    if (x > state->max_val) state->max_val = x;
    fixed_t delta = x - state->mean;
    state->mean += delta / state->n;
    fixed_t delta2 = x - state->mean;
    state->m2 += fixed_mul(delta, delta2);
}

fixed_t welford_get_mean(const WelfordState_t* state) {
    return state->mean;
}

fixed_t welford_get_variance(const WelfordState_t* state) {
    if (state->n < 2) return 0;
    return state->m2 / (state->n - 1);
}

fixed_t stats_mean(const fixed_t* data, uint32_t n) {
    if (n == 0) return 0;
    fixed_t sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += data[i];
    return sum / n;
}

fixed_t stats_variance(const fixed_t* data, uint32_t n) {
    if (n < 2) return 0;
    fixed_t mean = stats_mean(data, n);
    fixed_t sum_sq = 0;
    for (uint32_t i = 0; i < n; i++) {
        fixed_t diff = data[i] - mean;
        sum_sq += fixed_mul(diff, diff);
    }
    return sum_sq / (n - 1);
}

void test_statistics() {
    TEST_SUITE("Statistics");
    
    // Test data
    fixed_t data[10];
    for (int i = 0; i < 10; i++) {
        data[i] = INT_TO_FIXED(i + 1);  // 1, 2, 3, ..., 10
    }
    
    // Test Welford's algorithm
    WelfordState_t state;
    welford_init(&state);
    
    for (int i = 0; i < 10; i++) {
        welford_add_sample(&state, data[i]);
    }
    
    fixed_t mean = welford_get_mean(&state);
    fixed_t variance = welford_get_variance(&state);
    
    // Expected: mean = 5.5, variance = 9.166...
    float mean_f = FIXED_TO_FLOAT(mean);
    float variance_f = FIXED_TO_FLOAT(variance);
    
    TEST_ASSERT_EQ_FLOAT(5.5f, mean_f, 0.1f, "Welford mean calculation");
    TEST_ASSERT_EQ_FLOAT(9.166f, variance_f, 0.5f, "Welford variance calculation");
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(1), state.min_val, "Welford min value");
    TEST_ASSERT_EQ_INT(INT_TO_FIXED(10), state.max_val, "Welford max value");
    
    // Test standard statistics functions
    fixed_t std_mean = stats_mean(data, 10);
    fixed_t std_var = stats_variance(data, 10);
    
    TEST_ASSERT_EQ_INT(mean, std_mean, "Standard mean matches Welford");
    TEST_ASSERT_EQ_INT(variance, std_var, "Standard variance matches Welford");
    
    printf("  Mean: %.2f, Variance: %.2f\n", mean_f, variance_f);
}

// ============================================================================
// CLUSTERING TESTS
// ============================================================================

typedef struct {
    fixed_t x;
    fixed_t y;
    uint8_t cluster_id;
} DataPoint_t;

typedef struct {
    DataPoint_t points[100];
    uint16_t count;
} Dataset_t;

typedef struct {
    fixed_t x;
    fixed_t y;
    uint16_t point_count;
    fixed_t inertia;
} ClusterCentroid_t;

typedef struct {
    ClusterCentroid_t centroids[3];
    uint8_t k;
    uint8_t iterations;
    bool converged;
    fixed_t total_inertia;
    fixed_t silhouette_score;
} KMeansResult_t;

fixed_t clustering_distance_euclidean(const DataPoint_t* a, const DataPoint_t* b) {
    fixed_t dx = a->x - b->x;
    fixed_t dy = a->y - b->y;
    fixed_t sum_sq = fixed_mul(dx, dx) + fixed_mul(dy, dy);
    return fixed_sqrt(sum_sq);
}

void test_clustering() {
    TEST_SUITE("Clustering");
    
    // Create test dataset with 2 clear clusters
    Dataset_t dataset;
    dataset.count = 6;
    
    // Cluster 1: around (10, 10)
    dataset.points[0].x = INT_TO_FIXED(9);  dataset.points[0].y = INT_TO_FIXED(9);
    dataset.points[1].x = INT_TO_FIXED(10); dataset.points[1].y = INT_TO_FIXED(10);
    dataset.points[2].x = INT_TO_FIXED(11); dataset.points[2].y = INT_TO_FIXED(11);
    
    // Cluster 2: around (20, 20)
    dataset.points[3].x = INT_TO_FIXED(19); dataset.points[3].y = INT_TO_FIXED(19);
    dataset.points[4].x = INT_TO_FIXED(20); dataset.points[4].y = INT_TO_FIXED(20);
    dataset.points[5].x = INT_TO_FIXED(21); dataset.points[5].y = INT_TO_FIXED(21);
    
    // Test distance calculation
    fixed_t dist = clustering_distance_euclidean(&dataset.points[0], &dataset.points[1]);
    float dist_f = FIXED_TO_FLOAT(dist);
    TEST_ASSERT_EQ_FLOAT(1.414f, dist_f, 0.1f, "Euclidean distance calculation");
    
    // Test that intra-cluster distances are small
    fixed_t intra_dist = clustering_distance_euclidean(&dataset.points[0], &dataset.points[2]);
    fixed_t inter_dist = clustering_distance_euclidean(&dataset.points[0], &dataset.points[3]);
    
    TEST_ASSERT(intra_dist < inter_dist, "Intra-cluster < inter-cluster distance");
    
    printf("  Intra-cluster distance: %.2f\n", FIXED_TO_FLOAT(intra_dist));
    printf("  Inter-cluster distance: %.2f\n", FIXED_TO_FLOAT(inter_dist));
}

// ============================================================================
// THREAT MODEL TESTS
// ============================================================================

float calculate_entropy(const uint8_t* data, uint32_t len) {
    uint32_t frequencies[256] = {0};
    for (uint32_t i = 0; i < len; i++) {
        frequencies[data[i]]++;
    }
    
    float entropy = 0.0f;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            float p = (float)frequencies[i] / len;
            entropy -= p * log2f(p);
        }
    }
    return entropy;
}

void test_threat_model() {
    TEST_SUITE("Threat Model");
    
    // Test high entropy (random-like data)
    uint8_t random_data[100];
    for (int i = 0; i < 100; i++) {
        random_data[i] = (uint8_t)(rand() % 256);
    }
    float random_entropy = calculate_entropy(random_data, 100);
    TEST_ASSERT(random_entropy > 7.0f, "Random data has high entropy");
    printf("  Random data entropy: %.2f bits/byte\n", random_entropy);
    
    // Test low entropy (static data)
    uint8_t static_data[100];
    memset(static_data, 0x42, 100);
    float static_entropy = calculate_entropy(static_data, 100);
    TEST_ASSERT(static_entropy < 0.1f, "Static data has low entropy");
    printf("  Static data entropy: %.2f bits/byte\n", static_entropy);
    
    // Test medium entropy (structured data)
    uint8_t structured_data[100];
    for (int i = 0; i < 100; i++) {
        structured_data[i] = (uint8_t)(i % 16);  // Only 16 unique values
    }
    float structured_entropy = calculate_entropy(structured_data, 100);
    TEST_ASSERT(structured_entropy > 3.0f && structured_entropy < 5.0f, 
                "Structured data has medium entropy");
    printf("  Structured data entropy: %.2f bits/byte\n", structured_entropy);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("========================================\n");
    printf("Flipper RF Lab - Unit Test Runner\n");
    printf("========================================\n");
    printf("Testing portable components (no Flipper SDK required)\n\n");
    
    srand((unsigned)time(NULL));
    
    // Run all test suites
    test_fixed_point_math();
    test_compression();
    test_statistics();
    test_clustering();
    test_threat_model();
    
    // Print summary
    printf("\n========================================\n");
    printf("TEST SUMMARY\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", stats.total_tests);
    printf("Passed:       %d (%.1f%%)\n", 
           stats.passed_tests, 
           (stats.passed_tests * 100.0) / stats.total_tests);
    printf("Failed:       %d (%.1f%%)\n", 
           stats.failed_tests,
           (stats.failed_tests * 100.0) / stats.total_tests);
    printf("========================================\n");
    
    if (stats.failed_tests == 0) {
        printf("✓ ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("✗ SOME TESTS FAILED\n");
        return 1;
    }
}
