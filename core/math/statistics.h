#ifndef STATISTICS_H
#define STATISTICS_H

#include "fixed_point.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// STATISTICAL ANALYSIS MODULE
// Welford's online algorithm and other statistical functions
// ============================================================================

#define STATISTICS_MAX_SAMPLES  1000
#define HISTOGRAM_MAX_BINS      256

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    uint32_t n;                 // Number of samples
    fixed_t mean;               // Running mean
    fixed_t m2;                 // Sum of squares of differences from mean
    fixed_t min_val;            // Minimum value
    fixed_t max_val;            // Maximum value
    fixed_t variance;           // Calculated variance
    fixed_t std_dev;            // Standard deviation
} WelfordState_t;

typedef struct {
    uint16_t bins[HISTOGRAM_MAX_BINS];
    uint16_t num_bins;
    fixed_t min_val;
    fixed_t max_val;
    fixed_t bin_width;
    uint32_t total_samples;
    uint16_t peak_bin;
    uint32_t peak_count;
} Histogram_t;

typedef struct {
    fixed_t x[STATISTICS_MAX_SAMPLES];
    fixed_t y[STATISTICS_MAX_SAMPLES];
    uint32_t n;
    fixed_t slope;
    fixed_t intercept;
    fixed_t r_squared;
    fixed_t correlation;
} LinearRegression_t;

// ============================================================================
// WELFORD'S ONLINE ALGORITHM
// ============================================================================

void welford_init(WelfordState_t* state);
void welford_add_sample(WelfordState_t* state, fixed_t x);
fixed_t welford_get_mean(const WelfordState_t* state);
fixed_t welford_get_variance(const WelfordState_t* state);
fixed_t welford_get_std_dev(const WelfordState_t* state);
void welford_finalize(WelfordState_t* state);

// ============================================================================
// HISTOGRAM
// ============================================================================

void histogram_init(Histogram_t* hist, uint16_t num_bins, fixed_t min_val, fixed_t max_val);
void histogram_add(Histogram_t* hist, fixed_t value);
void histogram_normalize(Histogram_t* hist);
uint16_t histogram_get_percentile(const Histogram_t* hist, uint8_t percentile);
fixed_t histogram_get_mode(const Histogram_t* hist);
fixed_t histogram_get_median(const Histogram_t* hist);

// ============================================================================
// LINEAR REGRESSION
// ============================================================================

void linear_regression_init(LinearRegression_t* reg);
void linear_regression_add_point(LinearRegression_t* reg, fixed_t x, fixed_t y);
void linear_regression_calculate(LinearRegression_t* reg);
fixed_t linear_regression_predict(const LinearRegression_t* reg, fixed_t x);
fixed_t linear_regression_get_correlation(const LinearRegression_t* reg);

// ============================================================================
// DESCRIPTIVE STATISTICS
// ============================================================================

fixed_t stats_mean(const fixed_t* data, uint32_t n);
fixed_t stats_variance(const fixed_t* data, uint32_t n);
fixed_t stats_std_dev(const fixed_t* data, uint32_t n);
fixed_t stats_median(fixed_t* data, uint32_t n);  // Modifies data array
fixed_t stats_mode(const fixed_t* data, uint32_t n);
fixed_t stats_range(const fixed_t* data, uint32_t n);
fixed_t stats_skewness(const fixed_t* data, uint32_t n);
fixed_t stats_kurtosis(const fixed_t* data, uint32_t n);

// ============================================================================
// CORRELATION AND COVARIANCE
// ============================================================================

fixed_t stats_correlation(const fixed_t* x, const fixed_t* y, uint32_t n);
fixed_t stats_covariance(const fixed_t* x, const fixed_t* y, uint32_t n);
void stats_cross_correlation(const fixed_t* x, const fixed_t* y, uint32_t n, 
                              fixed_t* result, uint32_t max_lag);

// ============================================================================
// FREQUENCY DOMAIN (Simplified FFT)
// ============================================================================

void stats_fft_magnitude(const fixed_t* time_data, uint32_t n, 
                         fixed_t* freq_magnitude);
void stats_dft_bin(const fixed_t* data, uint32_t n, uint32_t k, 
                    fixed_t* real, fixed_t* imag);

// ============================================================================
// FILTERING
// ============================================================================

typedef struct {
    fixed_t coeffs[8];
    fixed_t history[8];
    uint8_t order;
    uint8_t index;
} FIRFilter_t;

typedef struct {
    fixed_t a_coeffs[4];  // Denominator
    fixed_t b_coeffs[4];  // Numerator
    fixed_t x_history[4];  // Input history
    fixed_t y_history[4];  // Output history
    uint8_t order;
    uint8_t index;
} IIRFilter_t;

void fir_filter_init(FIRFilter_t* filter, const fixed_t* coeffs, uint8_t order);
fixed_t fir_filter_process(FIRFilter_t* filter, fixed_t input);
void iir_filter_init(IIRFilter_t* filter, const fixed_t* a, const fixed_t* b, uint8_t order);
fixed_t iir_filter_process(IIRFilter_t* filter, fixed_t input);
void moving_average_init(fixed_t* buffer, uint8_t size);
fixed_t moving_average_update(fixed_t* buffer, uint8_t size, uint8_t* index, fixed_t new_val);

// ============================================================================
// SPECIAL FUNCTIONS
// ============================================================================

fixed_t stats_erf(fixed_t x);
fixed_t stats_erfc(fixed_t x);
fixed_t stats_normal_cdf(fixed_t x, fixed_t mean, fixed_t std_dev);
fixed_t stats_normal_pdf(fixed_t x, fixed_t mean, fixed_t std_dev);
fixed_t stats_inverse_normal_cdf(fixed_t p);

// ============================================================================
// ENTROPY AND INFORMATION THEORY
// ============================================================================

fixed_t stats_shannon_entropy(const uint8_t* data, uint32_t len);
fixed_t stats_kullback_leibler(const fixed_t* p, const fixed_t* q, uint32_t n);
void stats_mutual_information(const uint8_t* x, const uint8_t* y, uint32_t len, 
                                fixed_t* mi, fixed_t* joint_entropy);

#ifdef __cplusplus
}
#endif

#endif // STATISTICS_H
