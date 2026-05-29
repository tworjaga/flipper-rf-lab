#ifndef TIMER_PRECISION_H
#define TIMER_PRECISION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DWT (Data Watchpoint and Trace) Cycle Counter
// Provides 1 microsecond precision timing on 64MHz STM32WB55RG
// ============================================================================

// DWT register addresses
#define DWT_CONTROL             (*(volatile uint32_t*)0xE0001000)
#define DWT_CYCCNT              (*(volatile uint32_t*)0xE0001004)
#define DWT_CPICNT              (*(volatile uint32_t*)0xE0001008)
#define DWT_EXCCNT              (*(volatile uint32_t*)0xE000100C)
#define DWT_SLEEPCNT            (*(volatile uint32_t*)0xE0001010)
#define DWT_LSUCNT              (*(volatile uint32_t*)0xE0001014)
#define DWT_FOLDCNT             (*(volatile uint32_t*)0xE0001018)
#define DWT_PCSR                (*(volatile uint32_t*)0xE000101C)

// DWT Control register bits
#define DWT_CTRL_CYCCNTENA      (1 << 0)
#define DWT_CTRL_CPIEVTENA      (1 << 17)
#define DWT_CTRL_EXCEVTENA      (1 << 18)
#define DWT_CTRL_SLEEPEVTENA    (1 << 19)
#define DWT_CTRL_LSUEVTENA      (1 << 20)
#define DWT_CTRL_FOLDEVTENA     (1 << 21)
#define DWT_CTRL_CYCTAP         (1 << 9)
#define DWT_CTRL_POSTPRESET     (1 << 1)
#define DWT_CTRL_POSTCNT        (1 << 5)

// CoreDebug registers
#define CoreDebug_DEMCR         (*(volatile uint32_t*)0xE000EDFC)
#define CoreDebug_DEMCR_TRCENA  (1 << 24)

// Timing constants for 64MHz clock
#define CYCLES_PER_MICROSECOND  64
#define CYCLES_PER_MILLISECOND  64000

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
void timer_precision_init(void);
void timer_precision_deinit(void);

// DWT Cycle Counter
void dwt_enable_cycle_counter(void);
void dwt_disable_cycle_counter(void);
void dwt_reset_cycle_counter(void);
uint32_t dwt_get_cycle_count(void);
uint32_t dwt_get_cycles_per_us(void);

// Microsecond timing
uint32_t timer_get_us(void);
uint32_t timer_get_elapsed_us(uint32_t start_us);
void timer_delay_us(uint32_t us);
void timer_delay_ns(uint32_t ns);

// High-precision measurements
uint32_t measure_pulse_width_us(uint32_t timeout_us);
uint32_t measure_interval_us(uint32_t* last_timestamp);

// ISR-safe timing (no mutex/locking)
uint32_t timer_get_us_isr(void);
void timer_delay_us_isr(uint32_t us);

// Performance monitoring
uint32_t dwt_get_active_cycles(void);
void dwt_reset_active_cycle_counter(void);
uint32_t calculate_cpu_load_percent(uint32_t active_cycles, uint32_t total_cycles);

// Timestamp management
typedef struct {
    uint32_t seconds;
    uint32_t microseconds;
} PreciseTimestamp_t;

void timer_get_timestamp(PreciseTimestamp_t* ts);
uint32_t timer_timestamp_diff_us(const PreciseTimestamp_t* start, const PreciseTimestamp_t* end);

// Interval measurement for fingerprinting
typedef struct {
    uint32_t last_timestamp;
    uint32_t interval_sum;
    uint32_t interval_sum_sq;
    uint32_t count;
    uint32_t min_interval;
    uint32_t max_interval;
} IntervalStatistics_t;

void interval_stats_init(IntervalStatistics_t* stats);
void interval_stats_add(IntervalStatistics_t* stats, uint32_t interval_us);
uint32_t interval_stats_get_mean(const IntervalStatistics_t* stats);
uint32_t interval_stats_get_variance(const IntervalStatistics_t* stats);
uint32_t interval_stats_get_std_dev(const IntervalStatistics_t* stats);

// Jitter measurement
typedef struct {
    uint32_t expected_interval;
    uint32_t jitter_sum;
    uint32_t jitter_count;
    uint32_t max_jitter;
} JitterMeasurement_t;

void jitter_measurement_init(JitterMeasurement_t* jm, uint32_t expected_interval_us);
void jitter_measurement_add(JitterMeasurement_t* jm, uint32_t actual_interval_us);
uint32_t jitter_measurement_get_avg(const JitterMeasurement_t* jm);
uint32_t jitter_measurement_get_max(const JitterMeasurement_t* jm);

// Allan variance calculation for clock stability
typedef struct {
    uint32_t timestamps[100];
    uint8_t index;
    uint8_t count;
    uint32_t tau;  // Measurement interval
} AllanVarianceState_t;

void allan_variance_init(AllanVarianceState_t* avs, uint32_t tau_ms);
void allan_variance_add(AllanVarianceState_t* avs, uint32_t timestamp_us);
uint32_t allan_variance_calculate(const AllanVarianceState_t* avs);

// Timeout handling
typedef struct {
    uint32_t start_time;
    uint32_t timeout_us;
    bool expired;
} PrecisionTimeout_t;

void timeout_init(PrecisionTimeout_t* timeout, uint32_t timeout_us);
bool timeout_check(PrecisionTimeout_t* timeout);
bool timeout_is_expired(const PrecisionTimeout_t* timeout);
uint32_t timeout_remaining_us(const PrecisionTimeout_t* timeout);

// Busy-wait with cycle counting (deterministic timing)
void busy_wait_cycles(uint32_t cycles);
void busy_wait_us(uint32_t us);

// Critical section timing
uint32_t critical_section_enter(void);
void critical_section_exit(uint32_t primask);

#ifdef __cplusplus
}
#endif

#endif // TIMER_PRECISION_H
