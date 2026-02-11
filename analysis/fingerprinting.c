#include "fingerprinting.h"
#include "../core/math/fixed_point.h"
#include "../core/math/statistics.h"
#include "../storage/sd_manager.h"
#include <string.h>

#define TAG "FINGERPRINT"

// Static state
static FingerprintCaptureState_t capture_state;
static DeviceDatabase_t device_database;
static TemporalDeviceRecord_t temporal_records[MAX_DEVICE_DB_ENTRIES];
static uint8_t num_temporal_records = 0;
static bool fingerprinting_initialized = false;

// Weighting factors for fingerprint comparison
static const uint8_t drift_weight = 30;      // 30% timing drift
static const uint8_t slope_weight = 25;      // 25% rise/fall slopes
static const uint8_t clock_weight = 20;      // 20% clock stability
static const uint8_t rssi_weight = 25;       // 25% RSSI signature

// Initialize fingerprinting engine
FuriStatus fingerprinting_engine_init(void) {
    if(fingerprinting_initialized) {
        return FuriStatusOk;
    }
    
    FURI_LOG_I(TAG, "Initializing fingerprinting engine");
    
    memset(&capture_state, 0, sizeof(capture_state));
    memset(&device_database, 0, sizeof(device_database));
    memset(temporal_records, 0, sizeof(temporal_records));
    num_temporal_records = 0;
    
    // Load device database from SD card
    // (Would implement in full version)
    
    fingerprinting_initialized = true;
    FURI_LOG_I(TAG, "Fingerprinting engine initialized");
    
    return FuriStatusOk;
}

void fingerprinting_engine_deinit(void) {
    if(!fingerprinting_initialized) return;
    
    // Save device database
    fingerprinting_save_database();
    
    fingerprinting_initialized = false;
}

// Start capture for fingerprinting
void fingerprinting_start_capture(void) {
    memset(&capture_state, 0, sizeof(capture_state));
    capture_state.state = FINGERPRINT_STATE_SAMPLING;
    capture_state.capture_start_time = furi_get_tick();
    
    FURI_LOG_I(TAG, "Started fingerprint capture");
}

void fingerprinting_stop_capture(void) {
    capture_state.state = FINGERPRINT_STATE_IDLE;
    FURI_LOG_I(TAG, "Stopped fingerprint capture");
}

bool fingerprinting_is_capturing(void) {
    return (capture_state.state == FINGERPRINT_STATE_SAMPLING);
}

// Process captured frame
void fingerprinting_process_frame(const Frame_t* frame) {
    if(capture_state.state != FINGERPRINT_STATE_SAMPLING) return;
    
    uint32_t now = frame->timestamp_us;
    
    // Calculate inter-frame interval
    if(capture_state.last_frame_timestamp > 0) {
        uint32_t interval = now - capture_state.last_frame_timestamp;
        
        if(capture_state.interval_count < FINGERPRINT_SAMPLE_COUNT) {
            capture_state.inter_frame_intervals[capture_state.interval_count++] = interval;
        }
    }
    capture_state.last_frame_timestamp = now;
    
    // Store symbol timing (simplified - would analyze actual symbol edges)
    if(capture_state.symbol_count < FINGERPRINT_SAMPLE_COUNT) {
        capture_state.symbol_timings[capture_state.symbol_count++] = frame->duration_us / frame->length;
    }
    
    // Update RSSI envelope
    uint8_t rssi_idx = capture_state.frames_captured % 16;
    capture_state.rssi_envelope[rssi_idx] = (uint8_t)(frame->rssi_dbm + 128);  // Offset to positive
    
    capture_state.frames_captured++;
    
    // Check if we have enough samples
    if(capture_state.frames_captured >= FINGERPRINT_SAMPLE_COUNT) {
        capture_state.state = FINGERPRINT_STATE_ANALYZING;
        fingerprinting_generate_fingerprint(&capture_state.current_fingerprint);
        
        FURI_LOG_I(TAG, "Fingerprint capture complete, %lu frames", capture_state.frames_captured);
    }
}

// Process RSSI sample for slope analysis
void fingerprinting_process_rssi_sample(uint8_t rssi, uint32_t timestamp_us) {
    if(capture_state.state != FINGERPRINT_STATE_SAMPLING) return;
    
    if(capture_state.rssi_sample_count < MAX_SLOPE_SAMPLES) {
        capture_state.rssi_samples[capture_state.rssi_sample_count] = rssi;
        capture_state.rssi_sample_count++;
        
        if(capture_state.rssi_sample_count == 1) {
            capture_state.rssi_sample_start = timestamp_us;
        }
    }
}

// Analyze timing drift
void fingerprinting_analyze_timing_drift(void) {
    if(capture_state.interval_count < 10) return;
    
    StatisticalSummary_t stats;
    fingerprinting_calc_statistics(capture_state.inter_frame_intervals,
                                  capture_state.interval_count,
                                  &stats);
    
    capture_state.current_fingerprint.drift_mean = stats.mean;
    capture_state.current_fingerprint.drift_variance = stats.variance;
}

// Analyze rise/fall slopes from RSSI samples
void fingerprinting_analyze_rise_fall_slopes(void) {
    if(capture_state.rssi_sample_count < 10) return;
    
    uint32_t total_rise = 0;
    uint32_t total_fall = 0;
    uint16_t rise_count = 0;
    uint16_t fall_count = 0;
    
    for(uint16_t i = 1; i < capture_state.rssi_sample_count; i++) {
        int16_t diff = (int16_t)capture_state.rssi_samples[i] - 
                       (int16_t)capture_state.rssi_samples[i-1];
        
        if(diff > 0) {
            total_rise += diff;
            rise_count++;
        } else if(diff < 0) {
            total_fall += -diff;
            fall_count++;
        }
    }
    
    if(rise_count > 0) {
        capture_state.current_fingerprint.rise_time_avg = 
            (uint16_t)(total_rise / rise_count);
    }
    
    if(fall_count > 0) {
        capture_state.current_fingerprint.fall_time_avg = 
            (uint16_t)(total_fall / fall_count);
    }
}

// Analyze clock stability
void fingerprinting_analyze_clock_stability(void) {
    if(capture_state.symbol_count < 10) return;
    
    StatisticalSummary_t stats;
    fingerprinting_calc_statistics(capture_state.symbol_timings,
                                  capture_state.symbol_count,
                                  &stats);
    
    // Calculate PPM deviation
    // ppm = (std_dev / mean) * 1,000,000
    if(stats.mean > 0) {
        uint32_t ppm = (stats.std_dev * 1000000) / stats.mean;
        if(ppm > 255) ppm = 255;  // Clamp to uint8_t
        capture_state.current_fingerprint.clock_stability_ppm = (uint8_t)ppm;
    }
}

// Analyze RSSI envelope
void fingerprinting_analyze_rssi_envelope(void) {
    // Copy 16-point envelope to fingerprint
    memcpy(capture_state.current_fingerprint.rssi_signature,
           capture_state.rssi_envelope,
           16);
}

// Generate complete fingerprint
void fingerprinting_generate_fingerprint(RFFingerprint_t* fingerprint) {
    // Run all analyses
    fingerprinting_analyze_timing_drift();
    fingerprinting_analyze_rise_fall_slopes();
    fingerprinting_analyze_clock_stability();
    fingerprinting_analyze_rssi_envelope();
    
    // Calculate hash
    fingerprint->unique_hash = fingerprinting_calculate_hash(fingerprint);
    
    // Copy to output
    memcpy(fingerprint, &capture_state.current_fingerprint, sizeof(RFFingerprint_t));
    
    capture_state.state = FINGERPRINT_STATE_MATCHING;
}

// Calculate fingerprint hash (CRC16)
uint16_t fingerprinting_calculate_hash(const RFFingerprint_t* fingerprint) {
    uint16_t crc = 0xFFFF;
    const uint8_t* data = (const uint8_t*)fingerprint;
    uint16_t len = sizeof(RFFingerprint_t) - 2;  // Exclude hash field itself
    
    for(uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;  // CRC-16-CCITT polynomial
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// Match fingerprint against database
uint8_t fingerprinting_match_device(const RFFingerprint_t* fingerprint,
                                    uint16_t* matched_device_id,
                                    RFFingerprint_t* matched_fingerprint) {
    uint8_t best_confidence = 0;
    uint16_t best_match = 0;
    
    for(uint16_t i = 0; i < device_database.count; i++) {
        uint8_t confidence = fingerprinting_calculate_similarity(
            fingerprint, &device_database.fingerprints[i]);
        
        if(confidence > best_confidence) {
            best_confidence = confidence;
            best_match = i;
        }
    }
    
    if(best_confidence >= FINGERPRINT_CONFIDENCE_LOW) {
        *matched_device_id = best_match;
        if(matched_fingerprint) {
            memcpy(matched_fingerprint, 
                   &device_database.fingerprints[best_match],
                   sizeof(RFFingerprint_t));
        }
        
        // Update temporal record
        fingerprinting_update_temporal_record(best_match, fingerprint);
    }
    
    return best_confidence;
}

// Calculate similarity between two fingerprints (0-100%)
uint8_t fingerprinting_calculate_similarity(const RFFingerprint_t* a,
                                          const RFFingerprint_t* b) {
    // Calculate weighted distance
    uint32_t distance = fingerprinting_weighted_distance(a, b);
    
    // Convert distance to confidence (inverse relationship)
    // Max expected distance is roughly 10000 for completely different devices
    uint32_t max_distance = 10000;
    
    if(distance >= max_distance) return 0;
    
    uint8_t confidence = (uint8_t)(100 - (distance * 100 / max_distance));
    return confidence;
}

// Calculate Euclidean distance
uint32_t fingerprinting_euclidean_distance(const RFFingerprint_t* a,
                                          const RFFingerprint_t* b) {
    uint64_t sum = 0;
    
    // Drift components
    int32_t drift_mean_diff = (int32_t)a->drift_mean - (int32_t)b->drift_mean;
    int32_t drift_var_diff = (int32_t)a->drift_variance - (int32_t)b->drift_variance;
    sum += (uint64_t)(drift_mean_diff * drift_mean_diff);
    sum += (uint64_t)(drift_var_diff * drift_var_diff);
    
    // Slope components
    int32_t rise_diff = (int32_t)a->rise_time_avg - (int32_t)b->rise_time_avg;
    int32_t fall_diff = (int32_t)a->fall_time_avg - (int32_t)b->fall_time_avg;
    sum += (uint64_t)(rise_diff * rise_diff);
    sum += (uint64_t)(fall_diff * fall_diff);
    
    // Clock stability
    int32_t clock_diff = (int32_t)a->clock_stability_ppm - (int32_t)b->clock_stability_ppm;
    sum += (uint64_t)(clock_diff * clock_diff) * 100;  // Weight heavily
    
    // RSSI signature
    for(uint8_t i = 0; i < 16; i++) {
        int32_t rssi_diff = (int32_t)a->rssi_signature[i] - (int32_t)b->rssi_signature[i];
        sum += (uint64_t)(rssi_diff * rssi_diff);
    }
    
    // Integer square root
    uint32_t result = 0;
    uint32_t add = 0x8000;
    for(int i = 0; i < 16; i++) {
        uint32_t temp = result | add;
        if(sum >= (uint64_t)temp * temp) {
            result = temp;
        }
        add >>= 1;
    }
    
    return result;
}

// Calculate Manhattan distance
uint32_t fingerprinting_manhattan_distance(const RFFingerprint_t* a,
                                           const RFFingerprint_t* b) {
    uint32_t sum = 0;
    
    sum += (uint32_t)abs((int32_t)a->drift_mean - (int32_t)b->drift_mean);
    sum += (uint32_t)abs((int32_t)a->drift_variance - (int32_t)b->drift_variance);
    sum += (uint32_t)abs((int32_t)a->rise_time_avg - (int32_t)b->rise_time_avg);
    sum += (uint32_t)abs((int32_t)a->fall_time_avg - (int32_t)b->fall_time_avg);
    sum += (uint32_t)abs((int32_t)a->clock_stability_ppm - (int32_t)b->clock_stability_ppm) * 10;
    
    for(uint8_t i = 0; i < 16; i++) {
        sum += (uint32_t)abs((int32_t)a->rssi_signature[i] - (int32_t)b->rssi_signature[i]);
    }
    
    return sum;
}

// Calculate weighted distance
uint32_t fingerprinting_weighted_distance(const RFFingerprint_t* a,
                                        const RFFingerprint_t* b) {
    uint32_t distance = 0;
    
    // Timing drift (30%)
    uint32_t drift_dist = (uint32_t)abs((int32_t)a->drift_mean - (int32_t)b->drift_mean);
    drift_dist += (uint32_t)abs((int32_t)a->drift_variance - (int32_t)b->drift_variance) / 10;
    distance += (drift_dist * drift_weight) / 100;
    
    // Rise/fall slopes (25%)
    uint32_t slope_dist = (uint32_t)abs((int32_t)a->rise_time_avg - (int32_t)b->rise_time_avg);
    slope_dist += (uint32_t)abs((int32_t)a->fall_time_avg - (int32_t)b->fall_time_avg);
    distance += (slope_dist * slope_weight) / 100;
    
    // Clock stability (20%)
    uint32_t clock_dist = (uint32_t)abs((int32_t)a->clock_stability_ppm - (int32_t)b->clock_stability_ppm);
    distance += (clock_dist * clock_weight) / 100;
    
    // RSSI signature (25%)
    uint32_t rssi_dist = 0;
    for(uint8_t i = 0; i < 16; i++) {
        rssi_dist += (uint32_t)abs((int32_t)a->rssi_signature[i] - (int32_t)b->rssi_signature[i]);
    }
    distance += (rssi_dist * rssi_weight) / 100;
    
    return distance;
}

// Add fingerprint to database
bool fingerprinting_add_to_database(const RFFingerprint_t* fingerprint,
                                    const char* device_name) {
    if(device_database.count >= MAX_DEVICE_DB_ENTRIES) {
        FURI_LOG_E(TAG, "Database full");
        return false;
    }
    
    uint16_t idx = device_database.count;
    memcpy(&device_database.fingerprints[idx], fingerprint, sizeof(RFFingerprint_t));
    strncpy(device_database.device_names[idx], device_name, 15);
    device_database.device_names[idx][15] = '\0';
    device_database.last_seen[idx] = furi_get_tick();
    device_database.match_count[idx] = 1;
    
    device_database.count++;
    
    FURI_LOG_I(TAG, "Added device %d: %s", idx, device_name);
    
    // Save to SD card
    sd_manager_export_fingerprint(fingerprint, device_name);
    
    return true;
}

// Remove from database
bool fingerprinting_remove_from_database(uint16_t device_id) {
    if(device_id >= device_database.count) return false;
    
    // Shift entries
    for(uint16_t i = device_id; i < device_database.count - 1; i++) {
        device_database.fingerprints[i] = device_database.fingerprints[i + 1];
        memcpy(device_database.device_names[i], 
               device_database.device_names[i + 1], 16);
        device_database.last_seen[i] = device_database.last_seen[i + 1];
        device_database.match_count[i] = device_database.match_count[i + 1];
    }
    
    device_database.count--;
    FURI_LOG_I(TAG, "Removed device %d from database", device_id);
    
    return true;
}

// Get database entry
const RFFingerprint_t* fingerprinting_get_database_entry(uint16_t device_id) {
    if(device_id >= device_database.count) return NULL;
    return &device_database.fingerprints[device_id];
}

// Get database count
uint16_t fingerprinting_get_database_count(void) {
    return device_database.count;
}

// Start learning mode
void fingerprinting_start_learning(const char* device_name) {
    fingerprinting_start_capture();
    capture_state.state = FINGERPRINT_STATE_LEARNING;
    
    FURI_LOG_I(TAG, "Started learning mode for: %s", device_name);
}

void fingerprinting_stop_learning(void) {
    if(capture_state.state == FINGERPRINT_STATE_LEARNING) {
        // Add captured fingerprint to database
        // (Would need device name from context)
        fingerprinting_stop_capture();
    }
}

// Reset capture state
void fingerprinting_reset_capture_state(void) {
    memset(&capture_state, 0, sizeof(capture_state));
}

// Get capture progress
uint8_t fingerprinting_get_progress_percent(void) {
    if(capture_state.state != FINGERPRINT_STATE_SAMPLING) return 100;
    
    return (uint8_t)((capture_state.frames_captured * 100) / FINGERPRINT_SAMPLE_COUNT);
}

// Get state string
const char* fingerprinting_get_state_string(void) {
    switch(capture_state.state) {
        case FINGERPRINT_STATE_IDLE: return "IDLE";
        case FINGERPRINT_STATE_SAMPLING: return "SAMPLING";
        case FINGERPRINT_STATE_ANALYZING: return "ANALYZING";
        case FINGERPRINT_STATE_MATCHING: return "MATCHING";
        case FINGERPRINT_STATE_LEARNING: return "LEARNING";
        default: return "UNKNOWN";
    }
}

// Calculate statistics
void fingerprinting_calc_statistics(const uint32_t* data,
                                    uint16_t count,
                                    StatisticalSummary_t* result) {
    if(count == 0) {
        memset(result, 0, sizeof(StatisticalSummary_t));
        return;
    }
    
    // Find min, max, and calculate sum
    uint64_t sum = 0;
    result->min = data[0];
    result->max = data[0];
    
    for(uint16_t i = 0; i < count; i++) {
        sum += data[i];
        if(data[i] < result->min) result->min = data[i];
        if(data[i] > result->max) result->max = data[i];
    }
    
    result->mean = (uint32_t)(sum / count);
    
    // Calculate variance
    uint64_t variance_sum = 0;
    for(uint16_t i = 0; i < count; i++) {
        int64_t diff = (int64_t)data[i] - (int64_t)result->mean;
        variance_sum += diff * diff;
    }
    
    result->variance = (uint32_t)(variance_sum / count);
    
    // Calculate standard deviation (integer sqrt)
    uint32_t var = result->variance;
    uint32_t std_dev = 0;
    uint32_t add = 0x8000;
    for(int i = 0; i < 16; i++) {
        uint32_t temp = std_dev | add;
        if(var >= temp * temp) {
            std_dev = temp;
        }
        add >>= 1;
    }
    result->std_dev = std_dev;
    
    // Calculate median (simplified - would use sorting in full version)
    result->median = result->mean;
}

// Update temporal record
void fingerprinting_update_temporal_record(uint16_t device_id,
                                             const RFFingerprint_t* fingerprint) {
    // Find or create record
    TemporalDeviceRecord_t* record = NULL;
    for(uint8_t i = 0; i < num_temporal_records; i++) {
        if(temporal_records[i].device_id == device_id) {
            record = &temporal_records[i];
            break;
        }
    }
    
    if(!record && num_temporal_records < MAX_DEVICE_DB_ENTRIES) {
        record = &temporal_records[num_temporal_records++];
        record->device_id = device_id;
        record->first_seen = furi_get_tick();
        memcpy(&record->baseline, fingerprint, sizeof(RFFingerprint_t));
    }
    
    if(record) {
        // Add to history
        uint8_t idx = record->history_count % 10;
        memcpy(&record->history[idx], fingerprint, sizeof(RFFingerprint_t));
        record->history_count++;
        record->last_seen = furi_get_tick();
        record->match_count++;
    }
}

// Check for temporal drift
bool fingerprinting_check_drift(uint16_t device_id,
                                const RFFingerprint_t* current,
                                uint8_t* drift_percent) {
    TemporalDeviceRecord_t* record = NULL;
    for(uint8_t i = 0; i < num_temporal_records; i++) {
        if(temporal_records[i].device_id == device_id) {
            record = &temporal_records[i];
            break;
        }
    }
    
    if(!record) {
        *drift_percent = 0;
        return false;
    }
    
    // Calculate drift from baseline
    uint32_t distance = fingerprinting_euclidean_distance(&record->baseline, current);
    
    // Convert to percentage (arbitrary scaling)
    *drift_percent = (uint8_t)(distance / 100);
    if(*drift_percent > 100) *drift_percent = 100;
    
    record->drift_detected = (*drift_percent > 20);  // 20% threshold
    record->drift_magnitude = *drift_percent;
    
    return record->drift_detected;
}

// Detect counterfeit device
void fingerprinting_detect_counterfeit(const RFFingerprint_t* fingerprint,
                                       const char* claimed_device,
                                       uint8_t* confidence) {
    // Find claimed device in database
    uint16_t claimed_id = 0xFFFF;
    for(uint16_t i = 0; i < device_database.count; i++) {
        if(strcmp(device_database.device_names[i], claimed_device) == 0) {
            claimed_id = i;
            break;
        }
    }
    
    if(claimed_id == 0xFFFF) {
        *confidence = 0;  // Device not found
        return;
    }
    
    // Compare against claimed fingerprint
    uint8_t match_confidence = fingerprinting_calculate_similarity(
        fingerprint, &device_database.fingerprints[claimed_id]);
    
    // Also check against all other devices
    uint8_t best_other_match = 0;
    for(uint16_t i = 0; i < device_database.count; i++) {
        if(i == claimed_id) continue;
        
        uint8_t other_confidence = fingerprinting_calculate_similarity(
            fingerprint, &device_database.fingerprints[i]);
        
        if(other_confidence > best_other_match) {
            best_other_match = other_confidence;
        }
    }
    
    // If matches another device better than claimed, likely counterfeit
    if(best_other_match > match_confidence) {
        *confidence = 0;  // Counterfeit detected
    } else {
        *confidence = match_confidence;
    }
}

// Save database to SD
void fingerprinting_save_database(void) {
    // Would implement full database serialization
    FURI_LOG_I(TAG, "Saving fingerprint database (%d devices)", device_database.count);
}

// Track temporal drift
void fingerprinting_track_temporal_drift(uint16_t device_id,
                                       const RFFingerprint_t* new_fingerprint) {
    uint8_t drift;
    if(fingerprinting_check_drift(device_id, new_fingerprint, &drift)) {
        FURI_LOG_W(TAG, "Device %d showing %d%% temporal drift", device_id, drift);
    }
}
