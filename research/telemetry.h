#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INTERNAL TELEMETRY SYSTEM
// OS-level monitoring for reliability and optimization
// ============================================================================

#define TELEMETRY_BUFFER_SIZE       256
#define TELEMETRY_EVENT_NAME_LEN    16
#define TELEMETRY_MAX_COUNTERS      16

// Event types
typedef enum {
    TELEM_EVENT_BOOT = 0,
    TELEM_EVENT_ERROR,
    TELEM_EVENT_MODE_CHANGE,
    TELEM_EVENT_CAPTURE_START,
    TELEM_EVENT_CAPTURE_STOP,
    TELEM_EVENT_FRAME_DETECTED,
    TELEM_EVENT_BUFFER_OVERFLOW,
    TELEM_EVENT_SD_WRITE,
    TELEM_EVENT_SD_ERROR,
    TELEM_EVENT_LOW_BATTERY,
    TELEM_EVENT_TEMP_WARNING,
    TELEM_EVENT_CUSTOM
} TelemetryEventType_t;

// Event structure
typedef struct {
    TelemetryEventType_t type;
    uint32_t timestamp_ms;
    uint32_t uptime_ms;
    char name[TELEMETRY_EVENT_NAME_LEN];
    int32_t value;
    uint32_t context;
} TelemetryEvent_t;

// Performance counter
typedef struct {
    const char* name;
    uint32_t count;
    uint32_t total_time_us;
    uint32_t max_time_us;
    uint32_t min_time_us;
    uint32_t last_time_us;
} PerformanceCounter_t;

// System telemetry state
typedef struct {
    // Event log (circular buffer)
    TelemetryEvent_t events[TELEMETRY_BUFFER_SIZE];
    uint16_t event_head;
    uint16_t event_count;
    
    // Performance counters
    PerformanceCounter_t counters[TELEMETRY_MAX_COUNTERS];
    uint8_t counter_count;
    
    // System metrics
    uint32_t boot_time_ms;
    uint32_t last_update_ms;
    
    // RF metrics
    uint32_t cc1101_irq_count;
    uint32_t frames_processed;
    uint32_t frames_dropped;
    uint32_t buffer_overflows;
    
    // CPU metrics
    uint32_t cpu_load_percent;
    uint32_t max_isr_latency_us;
    uint32_t avg_isr_latency_us;
    
    // Memory metrics
    uint32_t heap_used;
    uint32_t heap_free;
    uint32_t stack_used_max;
    
    // Storage metrics
    uint32_t sd_writes_total;
    uint32_t sd_write_latency_max_us;
    uint32_t sd_write_latency_avg_us;
    uint32_t sd_errors;
    
    // Buffer metrics
    uint32_t rx_fifo_utilization;
    uint32_t tx_fifo_utilization;
    uint32_t dma_buffer_fill;
    
    // RF throughput
    uint32_t bits_per_second;
    uint32_t frame_error_rate;
    uint32_t protocol_detection_rate;
    
} TelemetryState_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus telemetry_init(void);
void telemetry_deinit(void);

// Event logging
void telemetry_log_event(TelemetryEventType_t type, const char* name, 
                         int32_t value, uint32_t context);
void telemetry_log_error(const char* source, int32_t error_code);
void telemetry_log_mode_change(const char* mode_name);

// Performance counters
void telemetry_counter_init(const char* name, uint8_t* counter_id);
void telemetry_counter_start(uint8_t counter_id);
void telemetry_counter_end(uint8_t counter_id);
void telemetry_counter_increment(uint8_t counter_id);

// System metrics
void telemetry_update_system_metrics(void);
void telemetry_update_rf_metrics(uint32_t frames, uint32_t dropped, 
                                  uint32_t overflows);
void telemetry_update_cpu_load(uint32_t load_percent);
void telemetry_update_isr_latency(uint32_t latency_us);
void telemetry_update_buffer_stats(uint32_t rx_util, uint32_t tx_util, 
                                    uint32_t dma_fill);

// Storage metrics
void telemetry_log_sd_write(uint32_t latency_us, bool success);
void telemetry_update_throughput(uint32_t bps, uint32_t fer, uint32_t pdr);

// Query functions
const TelemetryState_t* telemetry_get_state(void);
uint16_t telemetry_get_recent_events(TelemetryEvent_t* buffer, uint16_t max_count);
void telemetry_get_counter_stats(uint8_t counter_id, uint32_t* count, 
                                  uint32_t* avg_time, uint32_t* max_time);

// Reporting
void telemetry_generate_report(char* buffer, uint32_t max_len);
bool telemetry_export_to_sd(const char* filename);
void telemetry_print_to_console(void);

// Alerting
void telemetry_set_alert_threshold(TelemetryEventType_t type, int32_t threshold);
bool telemetry_check_alerts(void);

// Real-time monitoring
void telemetry_start_monitoring(uint32_t interval_ms);
void telemetry_stop_monitoring(void);
bool telemetry_is_monitoring(void);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_H
