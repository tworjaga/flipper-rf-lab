#include "timer_precision.h"

// Static state
static volatile bool timer_initialized = false;
static volatile uint32_t cycles_per_us = CYCLES_PER_MICROSECOND;
static volatile uint32_t active_cycle_start = 0;
static volatile uint32_t active_cycle_total = 0;

// Initialize precision timing system
void timer_precision_init(void) {
    if(timer_initialized) return;
    
    // Enable DWT trace
    CoreDebug_DEMCR |= CoreDebug_DEMCR_TRCENA;
    
    // Enable cycle counter
    DWT_CONTROL |= DWT_CTRL_CYCCNTENA;
    
    // Reset cycle counter
    DWT_CYCCNT = 0;
    
    // Calculate actual cycles per microsecond based on system clock
    // For STM32WB55RG at 64MHz: 64 cycles per microsecond
    cycles_per_us = SystemCoreClock / 1000000;
    
    timer_initialized = true;
}

void timer_precision_deinit(void) {
    // Disable cycle counter
    DWT_CONTROL &= ~DWT_CTRL_CYCCNTENA;
    
    // Disable DWT trace
    CoreDebug_DEMCR &= ~CoreDebug_DEMCR_TRCENA;
    
    timer_initialized = false;
}

// Enable DWT cycle counter
void dwt_enable_cycle_counter(void) {
    DWT_CONTROL |= DWT_CTRL_CYCCNTENA;
}

// Disable DWT cycle counter
void dwt_disable_cycle_counter(void) {
    DWT_CONTROL &= ~DWT_CTRL_CYCCNTENA;
}

// Reset cycle counter to zero
void dwt_reset_cycle_counter(void) {
    DWT_CYCCNT = 0;
}

// Get current cycle count
uint32_t dwt_get_cycle_count(void) {
    return DWT_CYCCNT;
}

// Get cycles per microsecond
uint32_t dwt_get_cycles_per_us(void) {
    return cycles_per_us;
}

// Get current time in microseconds
uint32_t timer_get_us(void) {
    return DWT_CYCCNT / cycles_per_us;
}

// ISR-safe version (no function calls)
uint32_t timer_get_us_isr(void) {
    return DWT_CYCCNT / cycles_per_us;
}

// Get elapsed time since start timestamp
uint32_t timer_get_elapsed_us(uint32_t start_us) {
    uint32_t current = timer_get_us();
    return current - start_us;  // Handles overflow correctly for 32-bit
}

// Delay for specified microseconds (busy-wait for precision)
void timer_delay_us(uint32_t us) {
    if(us == 0) return;
    
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles_to_wait = us * cycles_per_us;
    
    while((DWT_CYCCNT - start) < cycles_to_wait) {
        // Busy wait - ensures deterministic timing
        __NOP();
    }
}

// ISR-safe delay
void timer_delay_us_isr(uint32_t us) {
    timer_delay_us(us);  // Same implementation, already ISR-safe
}

// Delay for nanoseconds (approximate, limited by clock resolution)
void timer_delay_ns(uint32_t ns) {
    if(ns == 0) return;
    
    // Convert ns to cycles (64MHz = 15.625ns per cycle)
    uint32_t cycles = (ns * cycles_per_us) / 1000;
    if(cycles == 0) cycles = 1;
    
    uint32_t start = DWT_CYCCNT;
    while((DWT_CYCCNT - start) < cycles) {
        __NOP();
    }
}

// Measure pulse width with timeout
uint32_t measure_pulse_width_us(uint32_t timeout_us) {
    uint32_t start = timer_get_us();
    uint32_t timeout_cycles = timeout_us * cycles_per_us;
    uint32_t start_cycles = DWT_CYCCNT;
    
    // Wait for pulse end or timeout
    while((DWT_CYCCNT - start_cycles) < timeout_cycles) {
        // Check pulse end condition here (would need GPIO state)
        // This is a template - actual implementation depends on hardware
    }
    
    return timer_get_elapsed_us(start);
}

// Measure interval between events
uint32_t measure_interval_us(uint32_t* last_timestamp) {
    uint32_t now = timer_get_us();
    uint32_t interval = now - *last_timestamp;
    *last_timestamp = now;
    return interval;
}

// Get active cycles for CPU load calculation
uint32_t dwt_get_active_cycles(void) {
    return active_cycle_total;
}

// Reset active cycle counter
void dwt_reset_active_cycle_counter(void) {
    active_cycle_total = 0;
    active_cycle_start = DWT_CYCCNT;
}

// Calculate CPU load percentage
uint32_t calculate_cpu_load_percent(uint32_t active_cycles, uint32_t total_cycles) {
    if(total_cycles == 0) return 0;
    return (active_cycles * 100) / total_cycles;
}

// Get precise timestamp
void timer_get_timestamp(PreciseTimestamp_t* ts) {
    uint32_t cycles = DWT_CYCCNT;
    uint32_t total_us = cycles / cycles_per_us;
    
    ts->seconds = total_us / 1000000;
    ts->microseconds = total_us % 1000000;
}

// Calculate difference between timestamps in microseconds
uint32_t timer_timestamp_diff_us(const PreciseTimestamp_t* start, const PreciseTimestamp_t* end) {
    uint32_t start_us = (start->seconds * 1000000) + start->microseconds;
    uint32_t end_us = (end->seconds * 1000000) + end->microseconds;
    return end_us - start_us;
}

// Interval statistics functions
void interval_stats_init(IntervalStatistics_t* stats) {
    memset(stats, 0, sizeof(IntervalStatistics_t));
    stats->min_interval = 0xFFFFFFFF;
    stats->max_interval = 0;
}

void interval_stats_add(IntervalStatistics_t* stats, uint32_t interval_us) {
    stats->interval_sum += interval_us;
    stats->interval_sum_sq += (interval_us * interval_us);
    stats->count++;
    
    if(interval_us < stats->min_interval) {
        stats->min_interval = interval_us;
    }
    if(interval_us > stats->max_interval) {
        stats->max_interval = interval_us;
    }
}

uint32_t interval_stats_get_mean(const IntervalStatistics_t* stats) {
    if(stats->count == 0) return 0;
    return stats->interval_sum / stats->count;
}

uint32_t interval_stats_get_variance(const IntervalStatistics_t* stats) {
    if(stats->count < 2) return 0;
    
    // Variance = E[X^2] - (E[X])^2
    uint64_t mean = stats->interval_sum / stats->count;
    uint64_t mean_sq = (stats->interval_sum_sq / stats->count);
    uint64_t variance = mean_sq - (mean * mean);
    
    return (uint32_t)variance;
}

uint32_t interval_stats_get_std_dev(const IntervalStatistics_t* stats) {
    uint32_t variance = interval_stats_get_variance(stats);
    // Integer square root
    uint32_t x = variance;
    uint32_t res = 0;
    uint32_t add = 0x8000;
    
    for(int i = 0; i < 16; i++) {
        uint32_t temp = res | add;
        if(x >= temp * temp) {
            res = temp;
        }
        add >>= 1;
    }
    
    return res;
}

// Jitter measurement functions
void jitter_measurement_init(JitterMeasurement_t* jm, uint32_t expected_interval_us) {
    memset(jm, 0, sizeof(JitterMeasurement_t));
    jm->expected_interval = expected_interval_us;
}

void jitter_measurement_add(JitterMeasurement_t* jm, uint32_t actual_interval_us) {
    uint32_t jitter = (actual_interval_us > jm->expected_interval) ? 
                      (actual_interval_us - jm->expected_interval) : 
                      (jm->expected_interval - actual_interval_us);
    
    jm->jitter_sum += jitter;
    jm->jitter_count++;
    
    if(jitter > jm->max_jitter) {
        jm->max_jitter = jitter;
    }
}

uint32_t jitter_measurement_get_avg(const JitterMeasurement_t* jm) {
    if(jm->jitter_count == 0) return 0;
    return jm->jitter_sum / jm->jitter_count;
}

uint32_t jitter_measurement_get_max(const JitterMeasurement_t* jm) {
    return jm->max_jitter;
}

// Allan variance calculation for clock stability
void allan_variance_init(AllanVarianceState_t* avs, uint32_t tau_ms) {
    memset(avs, 0, sizeof(AllanVarianceState_t));
    avs->tau = tau_ms * 1000;  // Convert to microseconds
}

void allan_variance_add(AllanVarianceState_t* avs, uint32_t timestamp_us) {
    avs->timestamps[avs->index] = timestamp_us;
    avs->index = (avs->index + 1) % 100;
    if(avs->count < 100) avs->count++;
}

uint32_t allan_variance_calculate(const AllanVarianceState_t* avs) {
    if(avs->count < 10) return 0;  // Need at least 10 samples
    
    // Simplified Allan variance calculation
    // AVAR = (1/2) * <(y[i+1] - y[i])^2>
    // where y[i] is the fractional frequency at interval i
    
    uint64_t sum_sq_diff = 0;
    uint32_t count = 0;
    
    for(uint8_t i = 0; i < avs->count - 1; i++) {
        uint8_t idx1 = (avs->index + i) % 100;
        uint8_t idx2 = (avs->index + i + 1) % 100;
        
        int32_t diff = (int32_t)avs->timestamps[idx2] - (int32_t)avs->timestamps[idx1];
        int32_t diff_from_tau = diff - (int32_t)avs->tau;
        
        sum_sq_diff += (int64_t)diff_from_tau * (int64_t)diff_from_tau;
        count++;
    }
    
    if(count == 0) return 0;
    
    // Return scaled Allan variance
    return (uint32_t)(sum_sq_diff / (2 * count));
}

// Timeout handling functions
void timeout_init(PrecisionTimeout_t* timeout, uint32_t timeout_us) {
    timeout->start_time = timer_get_us();
    timeout->timeout_us = timeout_us;
    timeout->expired = false;
}

bool timeout_check(PrecisionTimeout_t* timeout) {
    if(timeout->expired) return true;
    
    uint32_t elapsed = timer_get_elapsed_us(timeout->start_time);
    if(elapsed >= timeout->timeout_us) {
        timeout->expired = true;
        return true;
    }
    return false;
}

bool timeout_is_expired(const PrecisionTimeout_t* timeout) {
    return timeout->expired;
}

uint32_t timeout_remaining_us(const PrecisionTimeout_t* timeout) {
    if(timeout->expired) return 0;
    
    uint32_t elapsed = timer_get_elapsed_us(timeout->start_time);
    if(elapsed >= timeout->timeout_us) return 0;
    return timeout->timeout_us - elapsed;
}

// Busy-wait for exact cycle count
void busy_wait_cycles(uint32_t cycles) {
    uint32_t start = DWT_CYCCNT;
    while((DWT_CYCCNT - start) < cycles) {
        __NOP();
    }
}

// Busy-wait for microseconds
void busy_wait_us(uint32_t us) {
    busy_wait_cycles(us * cycles_per_us);
}

// Critical section handling
uint32_t critical_section_enter(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void critical_section_exit(uint32_t primask) {
    __set_PRIMASK(primask);
}

// SystemCoreClock getter for modules that need it
uint32_t get_system_core_clock(void) {
    return SystemCoreClock;
}
