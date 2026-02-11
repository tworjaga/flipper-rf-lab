#ifndef FINGERPRINTING_H
#define FINGERPRINTING_H

#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// RF FINGERPRINTING ENGINE
// Device-level identification via RF imperfections
// ============================================================================

#define FINGERPRINT_SAMPLE_COUNT    1000    // Frames for drift analysis
#define RSSI_SAMPLE_RATE_HZ         100000  // 100kHz RSSI sampling
#define SLOPE_WINDOW_US             10      // Window for rise/fall measurement
#define MAX_SLOPE_SAMPLES           256     // Buffer for slope analysis

// Fingerprint confidence thresholds
#define FINGERPRINT_CONFIDENCE_HIGH     90  // 90%+ = high confidence match
#define FINGERPRINT_CONFIDENCE_MEDIUM   70  // 70-89% = medium confidence
#define FINGERPRINT_CONFIDENCE_LOW      50  // 50-69% = low confidence
#define FINGERPRINT_CONFIDENCE_NONE     0   // <50% = no match

// ============================================================================
// FINGERPRINTING STATE
// ============================================================================

typedef enum {
    FINGERPRINT_STATE_IDLE = 0,
    FINGERPRINT_STATE_SAMPLING,
    FINGERPRINT_STATE_ANALYZING,
    FINGERPRINT_STATE_MATCHING,
    FINGERPRINT_STATE_LEARNING
} FingerprintState_t;

typedef struct {
    // Timing analysis
    uint32_t inter_frame_intervals[FINGERPRINT_SAMPLE_COUNT];
    uint16_t interval_count;
    uint32_t last_frame_timestamp;
    
    // RSSI slope analysis
    uint8_t rssi_samples[MAX_SLOPE_SAMPLES];
    uint16_t rssi_sample_count;
    uint32_t rssi_sample_start;
    
    // Clock stability
    uint32_t symbol_timings[FINGERPRINT_SAMPLE_COUNT];
    uint16_t symbol_count;
    
    // RSSI envelope
    uint8_t rssi_envelope[16];  // 16-point characteristic curve
    
    // Working fingerprint
    RFFingerprint_t current_fingerprint;
    
    // State
    FingerprintState_t state;
    uint32_t frames_captured;
    uint32_t capture_start_time;
    
} FingerprintCaptureState_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus fingerprinting_engine_init(void);
void fingerprinting_engine_deinit(void);

// Capture control
void fingerprinting_start_capture(void);
void fingerprinting_stop_capture(void);
bool fingerprinting_is_capturing(void);

// Frame processing
void fingerprinting_process_frame(const Frame_t* frame);
void fingerprinting_process_rssi_sample(uint8_t rssi, uint32_t timestamp_us);

// Analysis functions
void fingerprinting_analyze_timing_drift(void);
void fingerprinting_analyze_rise_fall_slopes(void);
void fingerprinting_analyze_clock_stability(void);
void fingerprinting_analyze_rssi_envelope(void);

// Fingerprint generation
void fingerprinting_generate_fingerprint(RFFingerprint_t* fingerprint);
uint16_t fingerprinting_calculate_hash(const RFFingerprint_t* fingerprint);

// Matching
uint8_t fingerprinting_match_device(const RFFingerprint_t* fingerprint, 
                                    uint16_t* matched_device_id,
                                    RFFingerprint_t* matched_fingerprint);
uint8_t fingerprinting_calculate_similarity(const RFFingerprint_t* a, 
                                          const RFFingerprint_t* b);

// Database operations
bool fingerprinting_add_to_database(const RFFingerprint_t* fingerprint, 
                                    const char* device_name);
bool fingerprinting_remove_from_database(uint16_t device_id);
const RFFingerprint_t* fingerprinting_get_database_entry(uint16_t device_id);
uint16_t fingerprinting_get_database_count(void);

// Learning mode
void fingerprinting_start_learning(const char* device_name);
void fingerprinting_stop_learning(void);

// Utility functions
void fingerprinting_reset_capture_state(void);
uint8_t fingerprinting_get_progress_percent(void);
const char* fingerprinting_get_state_string(void);

// Advanced features
void fingerprinting_detect_counterfeit(const RFFingerprint_t* fingerprint,
                                       const char* claimed_device,
                                       uint8_t* confidence);
void fingerprinting_track_temporal_drift(uint16_t device_id,
                                         const RFFingerprint_t* new_fingerprint);

// ============================================================================
// DISTANCE METRICS FOR FINGERPRINT COMPARISON
// ============================================================================

uint32_t fingerprinting_euclidean_distance(const RFFingerprint_t* a, 
                                         const RFFingerprint_t* b);
uint32_t fingerprinting_manhattan_distance(const RFFingerprint_t* a, 
                                           const RFFingerprint_t* b);
uint32_t fingerprinting_weighted_distance(const RFFingerprint_t* a, 
                                          const RFFingerprint_t* b);

// ============================================================================
// STATISTICAL ANALYSIS
// ============================================================================

typedef struct {
    uint32_t mean;
    uint32_t variance;
    uint32_t std_dev;
    uint32_t min;
    uint32_t max;
    uint32_t median;
} StatisticalSummary_t;

void fingerprinting_calc_statistics(const uint32_t* data, 
                                    uint16_t count,
                                    StatisticalSummary_t* result);

// ============================================================================
// TEMPORAL TRACKING
// ============================================================================

typedef struct {
    uint16_t device_id;
    RFFingerprint_t baseline;
    RFFingerprint_t history[10];  // Last 10 fingerprints
    uint8_t history_count;
    uint32_t first_seen;
    uint32_t last_seen;
    uint32_t match_count;
    bool drift_detected;
    uint8_t drift_magnitude;
} TemporalDeviceRecord_t;

void fingerprinting_update_temporal_record(uint16_t device_id,
                                           const RFFingerprint_t* fingerprint);
bool fingerprinting_check_drift(uint16_t device_id,
                                const RFFingerprint_t* current,
                                uint8_t* drift_percent);

#ifdef __cplusplus
}
#endif

#endif // FINGERPRINTING_H
