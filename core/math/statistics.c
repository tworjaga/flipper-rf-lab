#include "statistics.h"
#include <string.h>

// Welford's online algorithm initialization
void welford_init(WelfordState_t* state) {
    memset(state, 0, sizeof(WelfordState_t));
    state->min_val = FIXED_MAX;
    state->max_val = FIXED_MIN;
}

// Add sample to Welford's algorithm
void welford_add_sample(WelfordState_t* state, fixed_t x) {
    state->n++;
    
    // Update min/max
    if(x < state->min_val) state->min_val = x;
    if(x > state->max_val) state->max_val = x;
    
    // Welford's online algorithm
    fixed_t delta = x - state->mean;
    state->mean += delta / state->n;
    fixed_t delta2 = x - state->mean;
    state->m2 += fixed_mul(delta, delta2);
}

// Get mean from Welford state
fixed_t welford_get_mean(const WelfordState_t* state) {
    return state->mean;
}

// Get variance from Welford state
fixed_t welford_get_variance(const WelfordState_t* state) {
    if(state->n < 2) return 0;
    return state->m2 / (state->n - 1);
}

// Get standard deviation from Welford state
fixed_t welford_get_std_dev(const WelfordState_t* state) {
    return fixed_sqrt(welford_get_variance(state));
}

// Finalize Welford calculations
void welford_finalize(WelfordState_t* state) {
    state->variance = welford_get_variance(state);
    state->std_dev = fixed_sqrt(state->variance);
}

// Initialize histogram
void histogram_init(Histogram_t* hist, uint16_t num_bins, fixed_t min_val, fixed_t max_val) {
    memset(hist, 0, sizeof(Histogram_t));
    hist->num_bins = (num_bins < HISTOGRAM_MAX_BINS) ? num_bins : HISTOGRAM_MAX_BINS;
    hist->min_val = min_val;
    hist->max_val = max_val;
    hist->bin_width = (max_val - min_val) / hist->num_bins;
    if(hist->bin_width == 0) hist->bin_width = FIXED_ONE;
}

// Add value to histogram
void histogram_add(Histogram_t* hist, fixed_t value) {
    if(value < hist->min_val || value > hist->max_val) return;
    
    uint16_t bin = (uint16_t)((value - hist->min_val) / hist->bin_width);
    if(bin >= hist->num_bins) bin = hist->num_bins - 1;
    
    hist->bins[bin]++;
    hist->total_samples++;
    
    // Update peak
    if(hist->bins[bin] > hist->peak_count) {
        hist->peak_count = hist->bins[bin];
        hist->peak_bin = bin;
    }
}

// Normalize histogram to probabilities
void histogram_normalize(Histogram_t* hist) {
    if(hist->total_samples == 0) return;
    
    for(uint16_t i = 0; i < hist->num_bins; i++) {
        // Scale to fixed-point probability (0-1)
        hist->bins[i] = (hist->bins[i] * FIXED_SCALE) / hist->total_samples;
    }
}

// Get percentile from histogram
uint16_t histogram_get_percentile(const Histogram_t* hist, uint8_t percentile) {
    uint32_t target = (hist->total_samples * percentile) / 100;
    uint32_t cumulative = 0;
    
    for(uint16_t i = 0; i < hist->num_bins; i++) {
        cumulative += hist->bins[i];
        if(cumulative >= target) {
            return i;
        }
    }
    
    return hist->num_bins - 1;
}

// Get mode (most frequent value)
fixed_t histogram_get_mode(const Histogram_t* hist) {
    return hist->min_val + (hist->peak_bin * hist->bin_width) + (hist->bin_width / 2);
}

// Get median from histogram
fixed_t histogram_get_median(const Histogram_t* hist) {
    uint16_t median_bin = histogram_get_percentile(hist, 50);
    return hist->min_val + (median_bin * hist->bin_width) + (hist->bin_width / 2);
}

// Initialize linear regression
void linear_regression_init(LinearRegression_t* reg) {
    memset(reg, 0, sizeof(LinearRegression_t));
}

// Add point to regression
void linear_regression_add_point(LinearRegression_t* reg, fixed_t x, fixed_t y) {
    if(reg->n >= STATISTICS_MAX_SAMPLES) return;
    
    reg->x[reg->n] = x;
    reg->y[reg->n] = y;
    reg->n++;
}

// Calculate linear regression
void linear_regression_calculate(LinearRegression_t* reg) {
    if(reg->n < 2) return;
    
    // Calculate means
    fixed_t mean_x = 0, mean_y = 0;
    for(uint32_t i = 0; i < reg->n; i++) {
        mean_x += reg->x[i];
        mean_y += reg->y[i];
    }
    mean_x /= reg->n;
    mean_y /= reg->n;
    
    // Calculate slope and intercept
    fixed_t num = 0, den = 0;
    for(uint32_t i = 0; i < reg->n; i++) {
        fixed_t dx = reg->x[i] - mean_x;
        fixed_t dy = reg->y[i] - mean_y;
        num += fixed_mul(dx, dy);
        den += fixed_mul(dx, dx);
    }
    
    if(den != 0) {
        reg->slope = fixed_div(num, den);
        reg->intercept = mean_y - fixed_mul(reg->slope, mean_x);
    }
    
    // Calculate R-squared
    fixed_t ss_res = 0, ss_tot = 0;
    for(uint32_t i = 0; i < reg->n; i++) {
        fixed_t y_pred = linear_regression_predict(reg, reg->x[i]);
        fixed_t res = reg->y[i] - y_pred;
        fixed_t tot = reg->y[i] - mean_y;
        ss_res += fixed_mul(res, res);
        ss_tot += fixed_mul(tot, tot);
    }
    
    if(ss_tot != 0) {
        reg->r_squared = FIXED_ONE - fixed_div(ss_res, ss_tot);
    }
    
    // Calculate correlation coefficient
    reg->correlation = fixed_sqrt(reg->r_squared);
    if(num < 0) reg->correlation = -reg->correlation;
}

// Predict Y from X using regression
fixed_t linear_regression_predict(const LinearRegression_t* reg, fixed_t x) {
    return reg->intercept + fixed_mul(reg->slope, x);
}

// Get correlation coefficient
fixed_t linear_regression_get_correlation(const LinearRegression_t* reg) {
    return reg->correlation;
}

// Calculate mean
fixed_t stats_mean(const fixed_t* data, uint32_t n) {
    if(n == 0) return 0;
    
    fixed_t sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        sum += data[i];
    }
    return sum / n;
}

// Calculate variance
fixed_t stats_variance(const fixed_t* data, uint32_t n) {
    if(n < 2) return 0;
    
    fixed_t mean = stats_mean(data, n);
    fixed_t sum_sq = 0;
    
    for(uint32_t i = 0; i < n; i++) {
        fixed_t diff = data[i] - mean;
        sum_sq += fixed_mul(diff, diff);
    }
    
    return sum_sq / (n - 1);
}

// Calculate standard deviation
fixed_t stats_std_dev(const fixed_t* data, uint32_t n) {
    return fixed_sqrt(stats_variance(data, n));
}

// Calculate median (modifies data array)
fixed_t stats_median(fixed_t* data, uint32_t n) {
    if(n == 0) return 0;
    if(n == 1) return data[0];
    
    // Simple bubble sort for small n
    for(uint32_t i = 0; i < n - 1; i++) {
        for(uint32_t j = 0; j < n - i - 1; j++) {
            if(data[j] > data[j + 1]) {
                fixed_t temp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = temp;
            }
        }
    }
    
    if(n % 2 == 0) {
        return (data[n/2 - 1] + data[n/2]) / 2;
    } else {
        return data[n/2];
    }
}

// Calculate mode
fixed_t stats_mode(const fixed_t* data, uint32_t n) {
    if(n == 0) return 0;
    
    // Simple frequency count (inefficient for large n)
    uint32_t max_count = 0;
    fixed_t mode = data[0];
    
    for(uint32_t i = 0; i < n; i++) {
        uint32_t count = 0;
        for(uint32_t j = 0; j < n; j++) {
            if(data[j] == data[i]) count++;
        }
        if(count > max_count) {
            max_count = count;
            mode = data[i];
        }
    }
    
    return mode;
}

// Calculate range
fixed_t stats_range(const fixed_t* data, uint32_t n) {
    if(n == 0) return 0;
    
    fixed_t min_val = data[0];
    fixed_t max_val = data[0];
    
    for(uint32_t i = 1; i < n; i++) {
        if(data[i] < min_val) min_val = data[i];
        if(data[i] > max_val) max_val = data[i];
    }
    
    return max_val - min_val;
}

// Calculate skewness
fixed_t stats_skewness(const fixed_t* data, uint32_t n) {
    if(n < 3) return 0;
    
    fixed_t mean = stats_mean(data, n);
    fixed_t std_dev = stats_std_dev(data, n);
    if(std_dev == 0) return 0;
    
    fixed_t sum_cubed = 0;
    for(uint32_t i = 0; i < n; i++) {
        fixed_t diff = data[i] - mean;
        sum_cubed += fixed_mul(fixed_mul(diff, diff), diff);
    }
    
    fixed_t n_fixed = INT_TO_FIXED(n);
    return fixed_div(sum_cubed / n, fixed_mul(fixed_mul(std_dev, std_dev), std_dev));
}

// Calculate kurtosis
fixed_t stats_kurtosis(const fixed_t* data, uint32_t n) {
    if(n < 4) return 0;
    
    fixed_t mean = stats_mean(data, n);
    fixed_t variance = stats_variance(data, n);
    if(variance == 0) return 0;
    
    fixed_t sum_fourth = 0;
    for(uint32_t i = 0; i < n; i++) {
        fixed_t diff = data[i] - mean;
        fixed_t diff_sq = fixed_mul(diff, diff);
        sum_fourth += fixed_mul(diff_sq, diff_sq);
    }
    
    fixed_t n_fixed = INT_TO_FIXED(n);
    fixed_t variance_sq = fixed_mul(variance, variance);
    return fixed_div(sum_fourth / n, variance_sq);
}

// Calculate correlation coefficient
fixed_t stats_correlation(const fixed_t* x, const fixed_t* y, uint32_t n) {
    if(n < 2) return 0;
    
    fixed_t mean_x = stats_mean(x, n);
    fixed_t mean_y = stats_mean(y, n);
    
    fixed_t num = 0, den_x = 0, den_y = 0;
    for(uint32_t i = 0; i < n; i++) {
        fixed_t dx = x[i] - mean_x;
        fixed_t dy = y[i] - mean_y;
        num += fixed_mul(dx, dy);
        den_x += fixed_mul(dx, dx);
        den_y += fixed_mul(dy, dy);
    }
    
    fixed_t den = fixed_mul(fixed_sqrt(den_x), fixed_sqrt(den_y));
    if(den == 0) return 0;
    
    return fixed_div(num, den);
}

// Calculate covariance
fixed_t stats_covariance(const fixed_t* x, const fixed_t* y, uint32_t n) {
    if(n < 2) return 0;
    
    fixed_t mean_x = stats_mean(x, n);
    fixed_t mean_y = stats_mean(y, n);
    
    fixed_t sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        sum += fixed_mul(x[i] - mean_x, y[i] - mean_y);
    }
    
    return sum / (n - 1);
}

// Cross-correlation
void stats_cross_correlation(const fixed_t* x, const fixed_t* y, uint32_t n,
                              fixed_t* result, uint32_t max_lag) {
    for(uint32_t lag = 0; lag < max_lag && lag < n; lag++) {
        fixed_t sum = 0;
        for(uint32_t i = 0; i < n - lag; i++) {
            sum += fixed_mul(x[i], y[i + lag]);
        }
        result[lag] = sum / (n - lag);
    }
}

// Initialize FIR filter
void fir_filter_init(FIRFilter_t* filter, const fixed_t* coeffs, uint8_t order) {
    memset(filter, 0, sizeof(FIRFilter_t));
    filter->order = (order < 8) ? order : 8;
    for(uint8_t i = 0; i < filter->order; i++) {
        filter->coeffs[i] = coeffs[i];
    }
}

// Process sample through FIR filter
fixed_t fir_filter_process(FIRFilter_t* filter, fixed_t input) {
    // Add new sample to history
    filter->history[filter->index] = input;
    filter->index = (filter->index + 1) % filter->order;
    
    // Calculate output
    fixed_t output = 0;
    for(uint8_t i = 0; i < filter->order; i++) {
        uint8_t idx = (filter->index + filter->order - 1 - i) % filter->order;
        output += fixed_mul(filter->coeffs[i], filter->history[idx]);
    }
    
    return output;
}

// Initialize IIR filter
void iir_filter_init(IIRFilter_t* filter, const fixed_t* a, const fixed_t* b, uint8_t order) {
    memset(filter, 0, sizeof(IIRFilter_t));
    filter->order = (order < 4) ? order : 4;
    for(uint8_t i = 0; i < filter->order; i++) {
        filter->a_coeffs[i] = a[i];
        filter->b_coeffs[i] = b[i];
    }
}

// Process sample through IIR filter
fixed_t iir_filter_process(IIRFilter_t* filter, fixed_t input) {
    // Store input
    filter->x_history[filter->index] = input;
    
    // Calculate output: y[n] = (b0*x[n] + b1*x[n-1] + ... - a1*y[n-1] - ...) / a0
    fixed_t output = 0;
    
    // Feedforward (numerator)
    for(uint8_t i = 0; i < filter->order; i++) {
        uint8_t idx = (filter->index + filter->order - i) % filter->order;
        output += fixed_mul(filter->b_coeffs[i], filter->x_history[idx]);
    }
    
    // Feedback (denominator)
    for(uint8_t i = 1; i < filter->order; i++) {
        uint8_t idx = (filter->index + filter->order - i) % filter->order;
        output -= fixed_mul(filter->a_coeffs[i], filter->y_history[idx]);
    }
    
    if(filter->a_coeffs[0] != 0) {
        output = fixed_div(output, filter->a_coeffs[0]);
    }
    
    // Store output
    filter->y_history[filter->index] = output;
    filter->index = (filter->index + 1) % filter->order;
    
    return output;
}

// Moving average
void moving_average_init(fixed_t* buffer, uint8_t size) {
    memset(buffer, 0, sizeof(fixed_t) * size);
}

fixed_t moving_average_update(fixed_t* buffer, uint8_t size, uint8_t* index, fixed_t new_val) {
    buffer[*index] = new_val;
    *index = (*index + 1) % size;
    
    fixed_t sum = 0;
    for(uint8_t i = 0; i < size; i++) {
        sum += buffer[i];
    }
    
    return sum / size;
}

// Error function approximation
fixed_t stats_erf(fixed_t x) {
    // Abramowitz and Stegun approximation
    fixed_t a1 = FLOAT_TO_FIXED(0.254829592f);
    fixed_t a2 = FLOAT_TO_FIXED(-0.284496736f);
    fixed_t a3 = FLOAT_TO_FIXED(1.421413741f);
    fixed_t a4 = FLOAT_TO_FIXED(-1.453152027f);
    fixed_t a5 = FLOAT_TO_FIXED(1.061405429f);
    fixed_t p = FLOAT_TO_FIXED(0.3275911f);
    
    fixed_t sign = (x < 0) ? -FIXED_ONE : FIXED_ONE;
    x = fixed_abs(x);
    
    fixed_t t = fixed_div(FIXED_ONE, FIXED_ONE + fixed_mul(p, x));
    
    fixed_t y = FIXED_ONE - fixed_mul(t, fixed_exp(
        -fixed_mul(x, x) +
        fixed_mul(a1, t) +
        fixed_mul(a2, fixed_mul(t, t)) +
        fixed_mul(a3, fixed_mul(fixed_mul(t, t), t)) +
        fixed_mul(a4, fixed_mul(fixed_mul(fixed_mul(t, t), t), t)) +
        fixed_mul(a5, fixed_mul(fixed_mul(fixed_mul(fixed_mul(t, t), t), t), t))
    ));
    
    return fixed_mul(sign, y);
}

// Complementary error function
fixed_t stats_erfc(fixed_t x) {
    return FIXED_ONE - stats_erf(x);
}

// Normal CDF
fixed_t stats_normal_cdf(fixed_t x, fixed_t mean, fixed_t std_dev) {
    if(std_dev == 0) return (x < mean) ? 0 : FIXED_ONE;
    
    fixed_t z = fixed_div(x - mean, std_dev);
    fixed_t sqrt2 = FLOAT_TO_FIXED(1.414213562f);
    return fixed_mul(stats_erf(fixed_div(z, sqrt2)) + FIXED_ONE, FLOAT_TO_FIXED(0.5f));
}

// Normal PDF
fixed_t stats_normal_pdf(fixed_t x, fixed_t mean, fixed_t std_dev) {
    if(std_dev == 0) return 0;
    
    fixed_t pi = FLOAT_TO_FIXED(3.141592653f);
    fixed_t sqrt2pi = fixed_sqrt(fixed_mul(pi, FLOAT_TO_FIXED(2.0f)));
    fixed_t diff = x - mean;
    fixed_t exponent = -fixed_div(fixed_mul(diff, diff), 
                                   fixed_mul(FIXED_TWO, fixed_mul(std_dev, std_dev)));
    
    return fixed_div(fixed_exp(exponent), fixed_mul(std_dev, sqrt2pi));
}

// Inverse normal CDF (approximation)
fixed_t stats_inverse_normal_cdf(fixed_t p) {
    if(p <= 0) return FLOAT_TO_FIXED(-6.0f);
    if(p >= FIXED_ONE) return FLOAT_TO_FIXED(6.0f);
    
    // Approximation using rational function
    fixed_t q = p - FLOAT_TO_FIXED(0.5f);
    fixed_t r = q * q;
    
    // Simplified approximation
    return fixed_mul(q, FLOAT_TO_FIXED(2.0f));
}

// Shannon entropy
fixed_t stats_shannon_entropy(const uint8_t* data, uint32_t len) {
    if(len == 0) return 0;
    
    uint32_t frequencies[256] = {0};
    for(uint32_t i = 0; i < len; i++) {
        frequencies[data[i]]++;
    }
    
    fixed_t entropy = 0;
    for(uint16_t i = 0; i < 256; i++) {
        if(frequencies[i] > 0) {
            fixed_t p = INT_TO_FIXED(frequencies[i]) / len;
            // -p * log2(p)
            entropy -= fixed_mul(p, fixed_log2_lut((uint8_t)(p >> 8)));
        }
    }
    
    return entropy;
}

// Kullback-Leibler divergence
fixed_t stats_kullback_leibler(const fixed_t* p, const fixed_t* q, uint32_t n) {
    fixed_t kl = 0;
    for(uint32_t i = 0; i < n; i++) {
        if(p[i] > 0 && q[i] > 0) {
            kl += fixed_mul(p[i], fixed_log(fixed_div(p[i], q[i])));
        }
    }
    return kl;
}

// Mutual information
void stats_mutual_information(const uint8_t* x, const uint8_t* y, uint32_t len,
                                fixed_t* mi, fixed_t* joint_entropy) {
    // Simplified implementation
    // Full version would calculate joint distribution
    
    fixed_t h_x = stats_shannon_entropy(x, len);
    fixed_t h_y = stats_shannon_entropy(y, len);
    
    // Approximate joint entropy as max of individual entropies
    *joint_entropy = (h_x > h_y) ? h_x : h_y;
    
    // Mutual information: I(X;Y) = H(X) + H(Y) - H(X,Y)
    *mi = h_x + h_y - *joint_entropy;
}

// Simplified FFT magnitude (placeholder)
void stats_fft_magnitude(const fixed_t* time_data, uint32_t n,
                         fixed_t* freq_magnitude) {
    // Placeholder - full FFT implementation would go here
    // For now, just copy data
    for(uint32_t i = 0; i < n && i < 64; i++) {
        freq_magnitude[i] = fixed_abs(time_data[i]);
    }
}

// DFT single bin calculation
void stats_dft_bin(const fixed_t* data, uint32_t n, uint32_t k,
                    fixed_t* real, fixed_t* imag) {
    *real = 0;
    *imag = 0;
    
    fixed_t two_pi = FLOAT_TO_FIXED(6.283185307f);
    fixed_t angle_step = fixed_div(two_pi * k, INT_TO_FIXED(n));
    
    for(uint32_t i = 0; i < n; i++) {
        fixed_t angle = angle_step * i;
        *real += fixed_mul(data[i], fixed_cos(angle));
        *imag -= fixed_mul(data[i], fixed_sin(angle));
    }
}
