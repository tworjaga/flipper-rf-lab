#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FIXED-POINT MATH LIBRARY
// Q15.16 format: 16-bit integer, 16-bit fraction
// Range: -32768.0 to 32767.9999847
// Resolution: 1/65536 ≈ 0.00001526
// ============================================================================

// Type definitions
typedef int32_t fixed_t;           // Q15.16 fixed-point
typedef int64_t fixed_dbl_t;       // Q31.32 for intermediate calculations

// Constants
#define FIXED_FRACTIONAL_BITS   16
#define FIXED_SCALE             (1 << FIXED_FRACTIONAL_BITS)  // 65536
#define FIXED_MASK              (FIXED_SCALE - 1)             // 0xFFFF

// Special values
#define FIXED_ZERO              0
#define FIXED_ONE               FIXED_SCALE
#define FIXED_HALF              (FIXED_SCALE >> 1)
#define FIXED_TWO               (FIXED_SCALE << 1)
#define FIXED_PI                205887        // 3.14159 * 65536
#define FIXED_E                 178145        // 2.71828 * 65536
#define FIXED_MAX               0x7FFFFFFF
#define FIXED_MIN               0x80000000

// Conversion macros
#define INT_TO_FIXED(x)         ((fixed_t)(x) << FIXED_FRACTIONAL_BITS)
#define FIXED_TO_INT(x)         ((x) >> FIXED_FRACTIONAL_BITS)
#define FLOAT_TO_FIXED(x)       ((fixed_t)((x) * FIXED_SCALE))
#define FIXED_TO_FLOAT(x)       ((float)(x) / FIXED_SCALE)

// Fractional part extraction
#define FIXED_FRAC_PART(x)      ((x) & FIXED_MASK)
#define FIXED_INT_PART(x)       ((x) >> FIXED_FRACTIONAL_BITS)

// ============================================================================
// BASIC OPERATIONS
// ============================================================================

// Initialize fixed-point library
void fixed_point_init(void);

// Addition and subtraction (no overflow check)
static inline fixed_t fixed_add(fixed_t a, fixed_t b) {
    return a + b;
}

static inline fixed_t fixed_sub(fixed_t a, fixed_t b) {
    return a - b;
}

// Multiplication with saturation
fixed_t fixed_mul(fixed_t a, fixed_t b);

// Division with saturation
fixed_t fixed_div(fixed_t a, fixed_t b);

// Absolute value
static inline fixed_t fixed_abs(fixed_t x) {
    return (x < 0) ? -x : x;
}

// Negation
static inline fixed_t fixed_neg(fixed_t x) {
    return -x;
}

// Minimum and maximum
static inline fixed_t fixed_min(fixed_t a, fixed_t b) {
    return (a < b) ? a : b;
}

static inline fixed_t fixed_max(fixed_t a, fixed_t b) {
    return (a > b) ? a : b;
}

// Clamp value to range
static inline fixed_t fixed_clamp(fixed_t x, fixed_t min, fixed_t max) {
    if(x < min) return min;
    if(x > max) return max;
    return x;
}

// ============================================================================
// ADVANCED OPERATIONS
// ============================================================================

// Square root using integer bit-shift algorithm (Newton-Raphson)
fixed_t fixed_sqrt(fixed_t x);

// Fast inverse square root (Quake III style)
fixed_t fixed_inv_sqrt(fixed_t x);

// Exponential function (e^x)
fixed_t fixed_exp(fixed_t x);

// Natural logarithm
fixed_t fixed_log(fixed_t x);

// Power function (x^y)
fixed_t fixed_pow(fixed_t base, fixed_t exp);

// Trigonometric functions (using CORDIC or lookup tables)
fixed_t fixed_sin(fixed_t x);   // x in radians
fixed_t fixed_cos(fixed_t x);   // x in radians
fixed_t fixed_tan(fixed_t x);   // x in radians

// Inverse trigonometric functions
fixed_t fixed_asin(fixed_t x);
fixed_t fixed_acos(fixed_t x);
fixed_t fixed_atan(fixed_t x);
fixed_t fixed_atan2(fixed_t y, fixed_t x);

// ============================================================================
// LOGARITHM APPROXIMATIONS (FOR ENTROPY CALCULATIONS)
// ============================================================================

// Fast log2 approximation using lookup table
// Input: 8-bit value (0-255), Output: Q15.16 log2
fixed_t fixed_log2_lut(uint8_t x);

// Fast natural log using log2 conversion
// ln(x) = log2(x) * ln(2)
static inline fixed_t fixed_fast_log(fixed_t x) {
    if(x <= 0) return FIXED_MIN;
    // Simplified: use log2 approximation
    return fixed_mul(fixed_log2_lut((uint8_t)(x >> 8)), FLOAT_TO_FIXED(0.693147f));
}

// Approximate log2 for values near 1.0
// log2(x) ≈ (x-1) * 0.94 for x near 1
static inline fixed_t fixed_log2_approx(fixed_t x) {
    fixed_t diff = x - FIXED_ONE;
    return fixed_mul(diff, FLOAT_TO_FIXED(0.94f));
}

// ============================================================================
// MATRIX OPERATIONS (MAX 8x8)
// ============================================================================

#define MAX_MATRIX_SIZE 8

typedef struct {
    fixed_t data[MAX_MATRIX_SIZE][MAX_MATRIX_SIZE];
    uint8_t rows;
    uint8_t cols;
} FixedMatrix_t;

// Matrix initialization
void matrix_init(FixedMatrix_t* m, uint8_t rows, uint8_t cols);
void matrix_init_identity(FixedMatrix_t* m, uint8_t size);
void matrix_init_zero(FixedMatrix_t* m, uint8_t rows, uint8_t cols);

// Basic operations
void matrix_add(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result);
void matrix_sub(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result);
void matrix_mul(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result);
void matrix_scale(const FixedMatrix_t* m, fixed_t scalar, FixedMatrix_t* result);

// Advanced operations
fixed_t matrix_determinant_2x2(const FixedMatrix_t* m);
fixed_t matrix_determinant_3x3(const FixedMatrix_t* m);
bool matrix_inverse_2x2(const FixedMatrix_t* m, FixedMatrix_t* result);
bool matrix_inverse_3x3(const FixedMatrix_t* m, FixedMatrix_t* result);
void matrix_transpose(const FixedMatrix_t* m, FixedMatrix_t* result);

// ============================================================================
// VECTOR OPERATIONS
// ============================================================================

typedef struct {
    fixed_t data[MAX_MATRIX_SIZE];
    uint8_t size;
} FixedVector_t;

// Vector initialization
void vector_init(FixedVector_t* v, uint8_t size);
void vector_init_zero(FixedVector_t* v, uint8_t size);

// Basic operations
void vector_add(const FixedVector_t* a, const FixedVector_t* b, FixedVector_t* result);
void vector_sub(const FixedVector_t* a, const FixedVector_t* b, FixedVector_t* result);
void vector_scale(const FixedVector_t* v, fixed_t scalar, FixedVector_t* result);

// Dot product and norms
fixed_t vector_dot(const FixedVector_t* a, const FixedVector_t* b);
fixed_t vector_norm_squared(const FixedVector_t* v);
fixed_t vector_norm(const FixedVector_t* v);

// Distance metrics
fixed_t vector_euclidean_distance(const FixedVector_t* a, const FixedVector_t* b);
fixed_t vector_manhattan_distance(const FixedVector_t* a, const FixedVector_t* b);
fixed_t vector_cosine_similarity(const FixedVector_t* a, const FixedVector_t* b);

// ============================================================================
// SPECIALIZED FUNCTIONS FOR RF ANALYSIS
// ============================================================================

// Calculate RSSI in dBm from fixed-point linear value
fixed_t fixed_rssi_to_dbm(fixed_t linear);

// Convert dBm to linear power
fixed_t fixed_dbm_to_linear(fixed_t dbm);

// Calculate power ratio in dB
fixed_t fixed_db_ratio(fixed_t power1, fixed_t power0);

// Frequency to wavelength conversion
fixed_t fixed_freq_to_wavelength(fixed_t freq_hz);

// ============================================================================
// SATURATION ARITHMETIC
// ============================================================================

// Saturating addition
fixed_t fixed_add_sat(fixed_t a, fixed_t b);

// Saturating subtraction
fixed_t fixed_sub_sat(fixed_t a, fixed_t b);

// Saturating multiplication
fixed_t fixed_mul_sat(fixed_t a, fixed_t b);

// ============================================================================
// ROUNDING AND CONVERSION
// ============================================================================

// Round to nearest integer
static inline fixed_t fixed_round(fixed_t x) {
    return (x + (FIXED_SCALE >> 1)) & ~FIXED_MASK;
}

// Floor (round down)
static inline fixed_t fixed_floor(fixed_t x) {
    return x & ~FIXED_MASK;
}

// Ceiling (round up)
static inline fixed_t fixed_ceil(fixed_t x) {
    return (x + FIXED_MASK) & ~FIXED_MASK;
}

// ============================================================================
// COMPARISON FUNCTIONS
// ============================================================================

static inline bool fixed_equal(fixed_t a, fixed_t b) {
    return a == b;
}

static inline bool fixed_less(fixed_t a, fixed_t b) {
    return a < b;
}

static inline bool fixed_greater(fixed_t a, fixed_t b) {
    return a > b;
}

static inline bool fixed_less_equal(fixed_t a, fixed_t b) {
    return a <= b;
}

static inline bool fixed_greater_equal(fixed_t a, fixed_t b) {
    return a >= b;
}

#ifdef __cplusplus
}
#endif

#endif // FIXED_POINT_H
