#include "telemetry.h"
#include "../storage/sd_manager.h"
#include <string.h>

#define TAG "TELEMETRY"

static TelemetryState_t telemetry_state;
static bool telemetry_initialized = false;
static bool monitoring_active = false;
static uint32_t monitoring_interval_ms = 1000;

// Initialize telemetry system
FuriStatus telemetry_init(void) {
    if(telemetry_initialized) return FuriStatusOk;
    
    memset(&telemetry_state, 0, sizeof(telemetry_state));
    telemetry_state.boot_time_ms = furi_get_tick();
    telemetry_state.last_update_ms = telemetry_state.boot_time_ms;
    
    // Initialize counters
    for(uint8_t i = 0; i < TELEMETRY_MAX_COUNTERS; i++) {
        telemetry_state.counters[i].min_time_us = 0xFFFFFFFF;
    }
    
    telemetry_initialized = true;
    
    telemetry_log_event(TELEM_EVENT_BOOT, "SYSTEM", 0, 0);
    
    FURI_LOG_I(TAG, "Telemetry initialized");
    return FuriStatusOk;
}

void telemetry_deinit(void) {
    telemetry_initialized = false;
    monitoring_active = false;
}

// Log an event
void telemetry_log_event(TelemetryEventType_t type, const char* name, 
                         int32_t value, uint32_t context) {
    if(!telemetry_initialized) return;
    
    uint16_t idx = telemetry_state.event_head;
    
    telemetry_state.events[idx].type = type;
    telemetry_state.events[idx].timestamp_ms = furi_get_tick();
    telemetry_state.events[idx].uptime_ms = telemetry_state.events[idx].timestamp_ms - 
                                           telemetry_state.boot_time_ms;
    strncpy(telemetry_state.events[idx].name, name, TELEMETRY_EVENT_NAME_LEN - 1);
    telemetry_state.events[idx].name[TELEMETRY_EVENT_NAME_LEN - 1] = '\0';
    telemetry_state.events[idx].value = value;
    telemetry_state.events[idx].context = context;
    
    telemetry_state.event_head = (telemetry_state.event_head + 1) % TELEMETRY_BUFFER_SIZE;
    if(telemetry_state.event_count < TELEMETRY_BUFFER_SIZE) {
        telemetry_state.event_count++;
    }
}

// Log error
void telemetry_log_error(const char* source, int32_t error_code) {
    telemetry_log_event(TELEM_EVENT_ERROR, source, error_code, 0);
    FURI_LOG_E(TAG, "Error from %s: %ld", source, error_code);
}

// Log mode change
void telemetry_log_mode_change(const char* mode_name) {
    telemetry_log_event(TELEM_EVENT_MODE_CHANGE, mode_name, 0, 0);
}

// Initialize performance counter
void telemetry_counter_init(const char* name, uint8_t* counter_id) {
    if(!telemetry_initialized || telemetry_state.counter_count >= TELEMETRY_MAX_COUNTERS) {
        *counter_id = 0xFF;
        return;
    }
    
    uint8_t id = telemetry_state.counter_count++;
    telemetry_state.counters[id].name = name;
    telemetry_state.counters[id].count = 0;
    telemetry_state.counters[id].total_time_us = 0;
    telemetry_state.counters[id].max_time_us = 0;
    telemetry_state.counters[id].min_time_us = 0xFFFFFFFF;
    
    *counter_id = id;
}

// Start timing
void telemetry_counter_start(uint8_t counter_id) {
    if(counter_id >= telemetry_state.counter_count) return;
    telemetry_state.counters[counter_id].last_time_us = timer_get_us();
}

// End timing
void telemetry_counter_end(uint8_t counter_id) {
    if(counter_id >= telemetry_state.counter_count) return;
    
    uint32_t elapsed = timer_get_us() - telemetry_state.counters[counter_id].last_time_us;
    PerformanceCounter_t* ctr = &telemetry_state.counters[counter_id];
    
    ctr->count++;
    ctr->total_time_us += elapsed;
    if(elapsed > ctr->max_time_us) ctr->max_time_us = elapsed;
    if(elapsed < ctr->min_time_us) ctr->min_time_us = elapsed;
}

// Increment counter
void telemetry_counter_increment(uint8_t counter_id) {
    if(counter_id >= telemetry_state.counter_count) return;
    telemetry_state.counters[counter_id].count++;
}

// Update system metrics
void telemetry_update_system_metrics(void) {
    if(!telemetry_initialized) return;
    
    telemetry_state.last_update_ms = furi_get_tick();
    
    // Would query FreeRTOS for heap/stack info
    // telemetry_state.heap_free = xPortGetFreeHeapSize();
}

// Update RF metrics
void telemetry_update_rf_metrics(uint32_t frames, uint32_t dropped, 
                                  uint32_t overflows) {
    telemetry_state.frames_processed = frames;
    telemetry_state.frames_dropped = dropped;
    telemetry_state.buffer_overflows = overflows;
}

// Update CPU load
void telemetry_update_cpu_load(uint32_t load_percent) {
    telemetry_state.cpu_load_percent = load_percent;
    
    if(load_percent > 80) {
        telemetry_log_event(TELEM_EVENT_CUSTOM, "HIGH_CPU", load_percent, 0);
    }
}

// Update ISR latency
void telemetry_update_isr_latency(uint32_t latency_us) {
    if(latency_us > telemetry_state.max_isr_latency_us) {
        telemetry_state.max_isr_latency_us = latency_us;
    }
    
    // Running average
    telemetry_state.avg_isr_latency_us = 
        (telemetry_state.avg_isr_latency_us * 9 + latency_us) / 10;
    
    if(latency_us > 50) {  // Alert threshold
        telemetry_log_event(TELEM_EVENT_ERROR, "ISR_LATENCY", latency_us, 0);
    }
}

// Update buffer stats
void telemetry_update_buffer_stats(uint32_t rx_util, uint32_t tx_util, 
                                    uint32_t dma_fill) {
    telemetry_state.rx_fifo_utilization = rx_util;
    telemetry_state.tx_fifo_utilization = tx_util;
    telemetry_state.dma_buffer_fill = dma_fill;
}

// Log SD write
void telemetry_log_sd_write(uint32_t latency_us, bool success) {
    telemetry_state.sd_writes_total++;
    
    if(!success) {
        telemetry_state.sd_errors++;
        telemetry_log_event(TELEM_EVENT_SD_ERROR, "SD_WRITE_FAIL", latency_us, 0);
        return;
    }
    
    if(latency_us > telemetry_state.sd_write_latency_max_us) {
        telemetry_state.sd_write_latency_max_us = latency_us;
    }
    
    // Running average
    telemetry_state.sd_write_latency_avg_us = 
        (telemetry_state.sd_write_latency_avg_us * 9 + latency_us) / 10;
}

// Update throughput metrics
void telemetry_update_throughput(uint32_t bps, uint32_t fer, uint32_t pdr) {
    telemetry_state.bits_per_second = bps;
    telemetry_state.frame_error_rate = fer;
    telemetry_state.protocol_detection_rate = pdr;
}

// Get telemetry state
const TelemetryState_t* telemetry_get_state(void) {
    return &telemetry_state;
}

// Get recent events
uint16_t telemetry_get_recent_events(TelemetryEvent_t* buffer, uint16_t max_count) {
    if(!telemetry_initialized || !buffer || max_count == 0) return 0;
    
    uint16_t count = 0;
    uint16_t idx = telemetry_state.event_head;
    
    // Go backwards through circular buffer
    for(uint16_t i = 0; i < telemetry_state.event_count && count < max_count; i++) {
        idx = (idx + TELEMETRY_BUFFER_SIZE - 1) % TELEMETRY_BUFFER_SIZE;
        memcpy(&buffer[count++], &telemetry_state.events[idx], sizeof(TelemetryEvent_t));
    }
    
    return count;
}

// Get counter stats
void telemetry_get_counter_stats(uint8_t counter_id, uint32_t* count, 
                                  uint32_t* avg_time, uint32_t* max_time) {
    if(counter_id >= telemetry_state.counter_count) {
        *count = 0;
        *avg_time = 0;
        *max_time = 0;
        return;
    }
    
    PerformanceCounter_t* ctr = &telemetry_state.counters[counter_id];
    *count = ctr->count;
    *avg_time = (ctr->count > 0) ? (ctr->total_time_us / ctr->count) : 0;
    *max_time = ctr->max_time_us;
}

// Generate text report
void telemetry_generate_report(char* buffer, uint32_t max_len) {
    if(!buffer || max_len == 0) return;
    
    int pos = 0;
    pos += snprintf(buffer + pos, max_len - pos,
        "=== RF RESEARCH PLATFORM TELEMETRY ===\n"
        "Uptime: %lu ms\n"
        "Events logged: %u\n\n"
        "RF METRICS:\n"
        "  Frames processed: %lu\n"
        "  Frames dropped: %lu\n"
        "  Buffer overflows: %lu\n"
        "  Bits/second: %lu\n"
        "  Frame error rate: %lu%%\n"
        "  Protocol detection: %lu%%\n\n"
        "SYSTEM METRICS:\n"
        "  CPU load: %lu%%\n"
        "  Max ISR latency: %lu us\n"
        "  Avg ISR latency: %lu us\n"
        "  RX FIFO util: %lu%%\n"
        "  TX FIFO util: %lu%%\n\n"
        "STORAGE METRICS:\n"
        "  SD writes: %lu\n"
        "  SD errors: %lu\n"
        "  Max write latency: %lu us\n"
        "  Avg write latency: %lu us\n\n"
        "PERFORMANCE COUNTERS:\n",
        furi_get_tick() - telemetry_state.boot_time_ms,
        telemetry_state.event_count,
        telemetry_state.frames_processed,
        telemetry_state.frames_dropped,
        telemetry_state.buffer_overflows,
        telemetry_state.bits_per_second,
        telemetry_state.frame_error_rate,
        telemetry_state.protocol_detection_rate,
        telemetry_state.cpu_load_percent,
        telemetry_state.max_isr_latency_us,
        telemetry_state.avg_isr_latency_us,
        telemetry_state.rx_fifo_utilization,
        telemetry_state.tx_fifo_utilization,
        telemetry_state.sd_writes_total,
        telemetry_state.sd_errors,
        telemetry_state.sd_write_latency_max_us,
        telemetry_state.sd_write_latency_avg_us
    );
    
    for(uint8_t i = 0; i < telemetry_state.counter_count && pos < (int)max_len - 100; i++) {
        PerformanceCounter_t* ctr = &telemetry_state.counters[i];
        uint32_t avg = (ctr->count > 0) ? (ctr->total_time_us / ctr->count) : 0;
        pos += snprintf(buffer + pos, max_len - pos,
            "  %s: count=%lu avg=%luus max=%luus\n",
            ctr->name, ctr->count, avg, ctr->max_time_us
        );
    }
    
    // Recent events
    pos += snprintf(buffer + pos, max_len - pos, "\nRECENT EVENTS:\n");
    uint16_t event_idx = telemetry_state.event_head;
    for(uint16_t i = 0; i < 10 && i < telemetry_state.event_count && pos < (int)max_len - 50; i++) {
        event_idx = (event_idx + TELEMETRY_BUFFER_SIZE - 1) % TELEMETRY_BUFFER_SIZE;
        TelemetryEvent_t* ev = &telemetry_state.events[event_idx];
        pos += snprintf(buffer + pos, max_len - pos,
            "  [%lu] %s: %s (val=%ld)\n",
            ev->uptime_ms, 
            ev->type == TELEM_EVENT_ERROR ? "ERR" :
            ev->type == TELEM_EVENT_BOOT ? "BOOT" :
            ev->type == TELEM_EVENT_MODE_CHANGE ? "MODE" : "EVENT",
            ev->name, ev->value
        );
    }
}

// Export to SD card
bool telemetry_export_to_sd(const char* filename) {
    char report[2048];
    telemetry_generate_report(report, sizeof(report));
    
    // Would write to SD via sd_manager
    UNUSED(filename);
    return true;
}

// Print to console
void telemetry_print_to_console(void) {
    char report[2048];
    telemetry_generate_report(report, sizeof(report));
    FURI_LOG_I(TAG, "%s", report);
}

// Set alert threshold
void telemetry_set_alert_threshold(TelemetryEventType_t type, int32_t threshold) {
    // Would store thresholds
    UNUSED(type);
    UNUSED(threshold);
}

// Check for alerts
bool telemetry_check_alerts(void) {
    // Would check all metrics against thresholds
    return (telemetry_state.cpu_load_percent > 80 ||
            telemetry_state.max_isr_latency_us > 50 ||
            telemetry_state.sd_errors > 0);
}

// Start monitoring
void telemetry_start_monitoring(uint32_t interval_ms) {
    monitoring_active = true;
    monitoring_interval_ms = interval_ms;
}

// Stop monitoring
void telemetry_stop_monitoring(void) {
    monitoring_active = false;
}

// Check if monitoring
bool telemetry_is_monitoring(void) {
    return monitoring_active;
}
