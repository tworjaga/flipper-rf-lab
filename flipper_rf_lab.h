#include "fixed_point.h"
#include <string.h>

// Log2 lookup table for 8-bit values (0-255)
// log2(x) * 65536 for x in range [1, 256]
static const uint16_t log2_lut[256] = {
    0,      // log2(0) = -infinity (special case, set to 0)
    0,      // log2(1) = 0
    65536,  // log2(2) = 1
    103872, // log2(3) = 1.585
    131072, // log2(4) = 2
    152304, // log2(5) = 2.322
    169792, // log2(6) = 2.585
    184688, // log2(7) = 2.807
    196608, // log2(8) = 3
    206624, // log2(9) = 3.170
    215424, // log2(10) = 3.322
    223296, // log2(11) = 3.459
    230432, // log2(12) = 3.585
    236976, // log2(13) = 3.700
    243040, // log2(14) = 3.807
    248720, // log2(15) = 3.907
    262144, // log2(16) = 4
    // ... (remaining values would be filled in)
    // For brevity, showing pattern - full table would have 256 entries
};

// Sine lookup table for CORDIC or direct lookup
// 256 entries covering 0 to 2π (0 to 65536 in fixed-point angle)
static const int16_t sin_lut[256] = {
    0, 402, 804, 1206, 1608, 2009, 2410, 2811,
    3212, 3612, 4011, 4410, 4808, 5205, 5602, 5998,
    // ... (full 256-entry table would continue)
};

// Initialize fixed-point library
void fixed_point_init(void) {
    // Verify lookup tables are properly initialized
    // In production, this would check table integrity
    
    // Initialize any runtime state if needed
}

// Multiplication with proper scaling
fixed_t fixed_mul(fixed_t a, fixed_t b) {
    // Use 64-bit intermediate to prevent overflow
    fixed_dbl_t temp = (fixed_dbl_t)a * (fixed_dbl_t)b;
    // Round to nearest: add 0.5 before shifting
    temp += (temp >= 0) ? (FIXED_SCALE >> 1) : -(FIXED_SCALE >> 1);
    return (fixed_t)(temp >> FIXED_FRACTIONAL_BITS);
}

// Division with proper scaling
fixed_t fixed_div(fixed_t a, fixed_t b) {
    if(b == 0) {
        // Return max value on division by zero
        return (a >= 0) ? FIXED_MAX : FIXED_MIN;
    }
    
    // Use 64-bit intermediate for precision
    fixed_dbl_t temp = ((fixed_dbl_t)a << FIXED_FRACTIONAL_BITS);
    // Round to nearest
    if(temp >= 0) {
        temp += (b >> 1);
    } else {
        temp -= (b >> 1);
    }
    return (fixed_t)(temp / b);
}

// Square root using Newton-Raphson method
fixed_t fixed_sqrt(fixed_t x) {
    if(x <= 0) return 0;
    
    // Initial guess: x / 2
    fixed_t guess = x >> 1;
    if(guess == 0) guess = FIXED_ONE;
    
    // Newton-Raphson iteration: guess = (guess + x/guess) / 2
    for(int i = 0; i < 8; i++) {  // 8 iterations for convergence
        fixed_t div_result = fixed_div(x, guess);
        fixed_t new_guess = (guess + div_result) >> 1;
        
        // Check for convergence
        if(fixed_abs(new_guess - guess) < 16) {  // Close enough
            break;
        }
        guess = new_guess;
    }
    
    return guess;
}

// Fast inverse square root (Quake III style)
fixed_t fixed_inv_sqrt(fixed_t x) {
    if(x <= 0) return 0;
    
    // Initial approximation using bit manipulation
    fixed_t x2 = x >> 1;
    fixed_t y = FIXED_ONE;
    
    // Newton-Raphson iteration: y = y * (1.5 - (x2 * y * y))
    for(int i = 0; i < 4; i++) {
        fixed_t y_sq = fixed_mul(y, y);
        fixed_t x2_y_sq = fixed_mul(x2, y_sq);
        fixed_t three_halfs = FLOAT_TO_FIXED(1.5f);
        fixed_t temp = three_halfs - x2_y_sq;
        y = fixed_mul(y, temp);
    }
    
    return y;
}

// Exponential function using Taylor series
fixed_t fixed_exp(fixed_t x) {
    // e^x = 1 + x + x^2/2! + x^3/3! + ...
    
    if(x == 0) return FIXED_ONE;
    
    // Clamp to prevent overflow
    if(x > FLOAT_TO_FIXED(11.0f)) return FIXED_MAX;  // e^11 is huge
    if(x < FLOAT_TO_FIXED(-11.0f)) return 0;        // e^-11 is tiny
    
    fixed_t result = FIXED_ONE;
    fixed_t term = FIXED_ONE;
    fixed_t x_n = x;
    
    // Calculate terms iteratively
    for(int n = 1; n < 12; n++) {
        term = fixed_div(x_n, INT_TO_FIXED(n));
        result = fixed_add(result, term);
        
        // Next power of x
        x_n = fixed_mul(x_n, x);
        
        // Early termination if term becomes negligible
        if(fixed_abs(term) < 16) break;  // Less than 0.0002
    }
    
    return result;
}

// Natural logarithm using log2 conversion
fixed_t fixed_log(fixed_t x) {
    if(x <= 0) return FIXED_MIN;
    
    // Find integer part (position of highest set bit)
    int32_t int_part = 0;
    fixed_t y = x;
    while(y >= FIXED_TWO) {
        y >>= 1;
        int_part++;
    }
    
    // Now y is in [1, 2), use log2 approximation
    // log2(y) ≈ (y-1) * 0.94 for y near 1
    fixed_t frac_part = fixed_mul((y - FIXED_ONE), FLOAT_TO_FIXED(0.94f));
    
    // Combine: log(x) = (int_part + frac_part) * ln(2)
    fixed_t log2_result = INT_TO_FIXED(int_part) + frac_part;
    fixed_t ln2 = FLOAT_TO_FIXED(0.693147f);
    
    return fixed_mul(log2_result, ln2);
}

// Power function: x^y = e^(y * ln(x))
fixed_t fixed_pow(fixed_t base, fixed_t exp) {
    if(base == 0) return 0;
    if(exp == 0) return FIXED_ONE;
    
    fixed_t log_base = fixed_log(base);
    fixed_t y = fixed_mul(exp, log_base);
    
    return fixed_exp(y);
}

// Sine using lookup table with linear interpolation
fixed_t fixed_sin(fixed_t x) {
    // Normalize angle to [0, 2π) in fixed-point
    // 2π ≈ 411774 in Q15.16 (2π * 65536)
    const fixed_t two_pi = FLOAT_TO_FIXED(6.283185f);
    
    // Wrap to [0, 2π)
    while(x < 0) x += two_pi;
    while(x >= two_pi) x -= two_pi;
    
    // Scale to table index: x * 256 / (2π) = x * 40.743
    fixed_t index_fp = fixed_mul(x, FLOAT_TO_FIXED(40.743f));
    int32_t index = FIXED_TO_INT(index_fp);
    fixed_t frac = FIXED_FRAC_PART(index_fp);
    
    // Clamp index
    if(index < 0) index = 0;
    if(index >= 255) index = 255;
    
    // Linear interpolation
    int16_t val1 = sin_lut[index];
    int16_t val2 = sin_lut[index + 1];
    int32_t diff = val2 - val1;
    
    fixed_t result = INT_TO_FIXED(val1) + fixed_mul(INT_TO_FIXED(diff), frac);
    return result >> 8;  // Adjust for table scaling
}

// Cosine: cos(x) = sin(x + π/2)
fixed_t fixed_cos(fixed_t x) {
    const fixed_t half_pi = FLOAT_TO_FIXED(1.570796f);
    return fixed_sin(x + half_pi);
}

// Tangent: tan(x) = sin(x) / cos(x)
fixed_t fixed_tan(fixed_t x) {
    fixed_t s = fixed_sin(x);
    fixed_t c = fixed_cos(x);
    
    if(c == 0) return FIXED_MAX;  // Avoid division by zero
    
    return fixed_div(s, c);
}

// Arcsine using polynomial approximation
fixed_t fixed_asin(fixed_t x) {
    // Clamp to [-1, 1]
    if(x < -FIXED_ONE) x = -FIXED_ONE;
    if(x > FIXED_ONE) x = FIXED_ONE;
    
    // asin(x) ≈ x + (x^3)/6 + (3*x^5)/40 + ...
    // Simplified approximation for |x| <= 1
    fixed_t x3 = fixed_mul(fixed_mul(x, x), x);
    fixed_t term = fixed_div(x3, INT_TO_FIXED(6));
    
    return x + term;
}

// Arccos: acos(x) = π/2 - asin(x)
fixed_t fixed_acos(fixed_t x) {
    const fixed_t half_pi = FLOAT_TO_FIXED(1.570796f);
    return half_pi - fixed_asin(x);
}

// Arctangent using CORDIC or polynomial
fixed_t fixed_atan(fixed_t x) {
    // atan(x) ≈ x - x^3/3 + x^5/5 - ...
    // For |x| <= 1
    
    bool negative = (x < 0);
    if(negative) x = -x;
    
    fixed_t result;
    if(x <= FIXED_ONE) {
        fixed_t x2 = fixed_mul(x, x);
        fixed_t x3 = fixed_mul(x2, x);
        fixed_t x5 = fixed_mul(x3, x2);
        
        result = x - fixed_div(x3, INT_TO_FIXED(3)) + fixed_div(x5, INT_TO_FIXED(5));
    } else {
        // For x > 1: atan(x) = π/2 - atan(1/x)
        const fixed_t half_pi = FLOAT_TO_FIXED(1.570796f);
        fixed_t inv_x = fixed_div(FIXED_ONE, x);
        fixed_t inv_result = fixed_atan(inv_x);
        result = half_pi - inv_result;
    }
    
    return negative ? -result : result;
}

// atan2: four-quadrant arctangent
fixed_t fixed_atan2(fixed_t y, fixed_t x) {
    if(x == 0) {
        if(y > 0) return FLOAT_TO_FIXED(1.570796f);   // π/2
        if(y < 0) return FLOAT_TO_FIXED(-1.570796f);  // -π/2
        return 0;  // Undefined, return 0
    }
    
    fixed_t ratio = fixed_div(y, x);
    fixed_t result = fixed_atan(ratio);
    
    // Adjust quadrant
    if(x < 0) {
        const fixed_t pi = FLOAT_TO_FIXED(3.141593f);
        if(y >= 0) {
            result += pi;  // Quadrant II
        } else {
            result -= pi;  // Quadrant III
        }
    }
    
    return result;
}

// Log2 lookup table function
fixed_t fixed_log2_lut(uint8_t x) {
    if(x == 0) return FIXED_MIN;  // -infinity approximation
    return (fixed_t)log2_lut[x] << (FIXED_FRACTIONAL_BITS - 16);
}

// Matrix initialization
void matrix_init(FixedMatrix_t* m, uint8_t rows, uint8_t cols) {
    memset(m, 0, sizeof(FixedMatrix_t));
    m->rows = (rows <= MAX_MATRIX_SIZE) ? rows : MAX_MATRIX_SIZE;
    m->cols = (cols <= MAX_MATRIX_SIZE) ? cols : MAX_MATRIX_SIZE;
}

void matrix_init_identity(FixedMatrix_t* m, uint8_t size) {
    matrix_init(m, size, size);
    for(uint8_t i = 0; i < size; i++) {
        m->data[i][i] = FIXED_ONE;
    }
}

void matrix_init_zero(FixedMatrix_t* m, uint8_t rows, uint8_t cols) {
    matrix_init(m, rows, cols);
    // Already zeroed by matrix_init
}

// Matrix addition
void matrix_add(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result) {
    uint8_t rows = (a->rows < b->rows) ? a->rows : b->rows;
    uint8_t cols = (a->cols < b->cols) ? a->cols : b->cols;
    
    result->rows = rows;
    result->cols = cols;
    
    for(uint8_t i = 0; i < rows; i++) {
        for(uint8_t j = 0; j < cols; j++) {
            result->data[i][j] = a->data[i][j] + b->data[i][j];
        }
    }
}

// Matrix subtraction
void matrix_sub(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result) {
    uint8_t rows = (a->rows < b->rows) ? a->rows : b->rows;
    uint8_t cols = (a->cols < b->cols) ? a->cols : b->cols;
    
    result->rows = rows;
    result->cols = cols;
    
    for(uint8_t i = 0; i < rows; i++) {
        for(uint8_t j = 0; j < cols; j++) {
            result->data[i][j] = a->data[i][j] - b->data[i][j];
        }
    }
}

// Matrix multiplication
void matrix_mul(const FixedMatrix_t* a, const FixedMatrix_t* b, FixedMatrix_t* result) {
    if(a->cols != b->rows) {
        // Invalid dimensions
        result->rows = 0;
        result->cols = 0;
        return;
    }
    
    result->rows = a->rows;
    result->cols = b->cols;
    
    for(uint8_t i = 0; i < result->rows; i++) {
        for(uint8_t j = 0; j < result->cols; j++) {
            fixed_t sum = 0;
            for(uint8_t k = 0; k < a->cols; k++) {
                sum += fixed_mul(a->data[i][k], b->data[k][j]);
            }
            result->data[i][j] = sum;
        }
    }
}

// Matrix scaling
void matrix_scale(const FixedMatrix_t* m, fixed_t scalar, FixedMatrix_t* result) {
    result->rows = m->rows;
    result->cols = m->cols;
    
    for(uint8_t i = 0; i < m->rows; i++) {
        for(uint8_t j = 0; j < m->cols; j++) {
            result->data[i][j] = fixed_mul(m->data[i][j], scalar);
        }
    }
}

// 2x2 determinant
fixed_t matrix_determinant_2x2(const FixedMatrix_t* m) {
    if(m->rows != 2 || m->cols != 2) return 0;
    
    // det = ad - bc
    fixed_t ad = fixed_mul(m->data[0][0], m->data[1][1]);
    fixed_t bc = fixed_mul(m->data[0][1], m->data[1][0]);
    
    return ad - bc;
}

// 3x3 determinant
fixed_t matrix_determinant_3x3(const FixedMatrix_t* m) {
    if(m->rows != 3 || m->cols != 3) return 0;
    
    // det = a(ei - fh) - b(di - fg) + c(dh - eg)
    fixed_t a = m->data[0][0], b = m->data[0][1], c = m->data[0][2];
    fixed_t d = m->data[1][0], e = m->data[1][1], f = m->data[1][2];
    fixed_t g = m->data[2][0], h = m->data[2][1], i = m->data[2][2];
    
    fixed_t ei = fixed_mul(e, i);
    fixed_t fh = fixed_mul(f, h);
    fixed_t di = fixed_mul(d, i);
    fixed_t fg = fixed_mul(f, g);
    fixed_t dh = fixed_mul(d, h);
    fixed_t eg = fixed_mul(e, g);
    
    fixed_t term1 = fixed_mul(a, ei - fh);
    fixed_t term2 = fixed_mul(b, di - fg);
    fixed_t term3 = fixed_mul(c, dh - eg);
    
    return term1 - term2 + term3;
}

// 2x2 matrix inverse
bool matrix_inverse_2x2(const FixedMatrix_t* m, FixedMatrix_t* result) {
    if(m->rows != 2 || m->cols != 2) return false;
    
    fixed_t det = matrix_determinant_2x2(m);
    if(det == 0) return false;  // Singular matrix
    
    // Inverse = (1/det) * [d, -b; -c, a]
    fixed_t inv_det = fixed_div(FIXED_ONE, det);
    
    result->rows = 2;
    result->cols = 2;
    result->data[0][0] = fixed_mul(m->data[1][1], inv_det);
    result->data[0][1] = fixed_mul(-m->data[0][1], inv_det);
    result->data[1][0] = fixed_mul(-m->data[1][0], inv_det);
    result->data[1][1] = fixed_mul(m->data[0][0], inv_det);
    
    return true;
}

// 3x3 matrix inverse (simplified)
bool matrix_inverse_3x3(const FixedMatrix_t* m, FixedMatrix_t* result) {
    if(m->rows != 3 || m->cols != 3) return false;
    
    fixed_t det = matrix_determinant_3x3(m);
    if(det == 0) return false;
    
    // For 3x3, we'd use cofactor expansion
    // This is a simplified version - full implementation would be longer
    // For now, return false to indicate not fully implemented
    return false;
}

// Matrix transpose
void matrix_transpose(const FixedMatrix_t* m, FixedMatrix_t* result) {
    result->rows = m->cols;
    result->cols = m->rows;
    
    for(uint8_t i = 0; i < m->rows; i++) {
        for(uint8_t j = 0; j < m->cols; j++) {
            result->data[j][i] = m->data[i][j];
        }
    }
}

// Vector initialization
void vector_init(FixedVector_t* v, uint8_t size) {
    v->size = (size <= MAX_MATRIX_SIZE) ? size : MAX_MATRIX_SIZE;
    memset(v->data, 0, sizeof(fixed_t) * v->size);
}

void vector_init_zero(FixedVector_t* v, uint8_t size) {
    vector_init(v, size);
}

// Vector addition
void vector_add(const FixedVector_t* a, const FixedVector_t* b, FixedVector_t* result) {
    uint8_t size = (a->size < b->size) ? a->size : b->size;
    result->size = size;
    
    for(uint8_t i = 0; i < size; i++) {
        result->data[i] = a->data[i] + b->data[i];
    }
}

// Vector subtraction
void vector_sub(const FixedVector_t* a, const FixedVector_t* b, FixedVector_t* result) {
    uint8_t size = (a->size < b->size) ? a->size : b->size;
    result->size = size;
    
    for(uint8_t i = 0; i < size; i++) {
        result->data[i] = a->data[i] - b->data[i];
    }
}

// Vector scaling
void vector_scale(const FixedVector_t* v, fixed_t scalar, FixedVector_t* result) {
    result->size = v->size;
    
    for(uint8_t i = 0; i < v->size; i++) {
        result->data[i] = fixed_mul(v->data[i], scalar);
    }
}

// Vector dot product
fixed_t vector_dot(const FixedVector_t* a, const FixedVector_t* b) {
    uint8_t size = (a->size < b->size) ? a->size : b->size;
    fixed_t sum = 0;
    
    for(uint8_t i = 0; i < size; i++) {
        sum += fixed_mul(a->data[i], b->data[i]);
    }
    
    return sum;
}

// Vector norm squared
fixed_t vector_norm_squared(const FixedVector_t* v) {
    return vector_dot(v, v);
}

// Vector norm (magnitude)
fixed_t vector_norm(const FixedVector_t* v) {
    return fixed_sqrt(vector_norm_squared(v));
}

// Euclidean distance
fixed_t vector_euclidean_distance(const FixedVector_t* a, const FixedVector_t* b) {
    FixedVector_t diff;
    vector_sub(a, b, &diff);
    return vector_norm(&diff);
}

// Manhattan distance
fixed_t vector_manhattan_distance(const FixedVector_t* a, const FixedVector_t* b) {
    uint8_t size = (a->size < b->size) ? a->size : b->size;
    fixed_t sum = 0;
    
    for(uint8_t i = 0; i < size; i++) {
        fixed_t diff = a->data[i] - b->data[i];
        sum += fixed_abs(diff);
    }
    
    return sum;
}

// Cosine similarity
fixed_t vector_cosine_similarity(const FixedVector_t* a, const FixedVector_t* b) {
    fixed_t dot = vector_dot(a, b);
    fixed_t norm_a = vector_norm(a);
    fixed_t norm_b = vector_norm(b);
    
    if(norm_a == 0 || norm_b == 0) return 0;
    
    fixed_t denom = fixed_mul(norm_a, norm_b);
    return fixed_div(dot, denom);
}

// Saturating addition
fixed_t fixed_add_sat(fixed_t a, fixed_t b) {
    fixed_t result = a + b;
    
    // Check for overflow
    if(b > 0 && result < a) return FIXED_MAX;
    if(b < 0 && result > a) return FIXED_MIN;
    
    return result;
}

// Saturating subtraction
fixed_t fixed_sub_sat(fixed_t a, fixed_t b) {
    fixed_t result = a - b;
    
    // Check for overflow
    if(b < 0 && result < a) return FIXED_MAX;
    if(b > 0 && result > a) return FIXED_MIN;
    
    return result;
}

// Saturating multiplication
fixed_t fixed_mul_sat(fixed_t a, fixed_t b) {
    fixed_dbl_t temp = (fixed_dbl_t)a * (fixed_dbl_t)b;
    temp >>= FIXED_FRACTIONAL_BITS;
    
    if(temp > FIXED_MAX) return FIXED_MAX;
    if(temp < FIXED_MIN) return FIXED_MIN;
    
    return (fixed_t)temp;
}

// RSSI to dBm conversion
fixed_t fixed_rssi_to_dbm(fixed_t linear) {
    // dBm = 10 * log10(linear)
    // log10(x) = log2(x) / log2(10) ≈ log2(x) / 3.3219
    if(linear <= 0) return FLOAT_TO_FIXED(-100.0f);  // Minimum RSSI
    
    fixed_t log2_val = fixed_log2_lut((uint8_t)(linear >> 8));
    fixed_t log10_val = fixed_div(log2_val, FLOAT_TO_FIXED(3.3219f));
    
    return fixed_mul(FLOAT_TO_FIXED(10.0f), log10_val);
}

// dBm to linear power conversion
fixed_t fixed_dbm_to_linear(fixed_t dbm) {
    // linear = 10^(dbm/10)
    fixed_t exponent = fixed_div(dbm, FLOAT_TO_FIXED(10.0f));
    return fixed_pow(FLOAT_TO_FIXED(10.0f), exponent);
}

// dB ratio calculation
fixed_t fixed_db_ratio(fixed_t power1, fixed_t power0) {
    // dB = 10 * log10(power1/power0)
    if(power0 == 0) return FIXED_MAX;
    
    fixed_t ratio = fixed_div(power1, power0);
    return fixed_rssi_to_dbm(ratio);
}

// Frequency to wavelength: λ = c / f
// c = 299,792,458 m/s
fixed_t fixed_freq_to_wavelength(fixed_t freq_hz) {
    const fixed_t speed_of_light = FLOAT_TO_FIXED(299792458.0f);
    return fixed_div(speed_of_light, freq_hz);
}
