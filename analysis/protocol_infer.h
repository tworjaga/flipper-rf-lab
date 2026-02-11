#ifndef PROTOCOL_INFER_H
#define PROTOCOL_INFER_H

#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ADAPTIVE SIGNAL MODELING ENGINE
// Protocol hypothesis generation for reverse engineering
// ============================================================================

#define MAX_PULSE_BINS          256
#define MAX_SYMBOL_TYPES        8
#define MAX_FRAME_SAMPLES       100
#define MAX_PREAMBLE_LEN        32

// Modulation types
typedef enum {
    MOD_UNKNOWN = 0,
    MOD_OOK,
    MOD_ASK,
    MOD_FSK,
    MOD_GFSK,
    MOD_MSK,
    MOD_PSK
} ModulationType_t;

// Encoding types
typedef enum {
    ENC_UNKNOWN = 0,
    ENC_NRZ,
    ENC_MANCHESTER,
    ENC_MANCHESTER_IEEE,
    ENC_MILLER,
    ENC_PWM,
    ENC_PPM,
    ENC_RZ
} EncodingType_t;

// Symbol alphabet entry
typedef struct {
    uint16_t width_us;
    uint16_t tolerance_us;
    uint8_t symbol_value;
    const char* name;
} SymbolAlphabet_t;

// Protocol hypothesis
typedef struct {
    ModulationType_t modulation;
    EncodingType_t encoding;
    uint32_t baud_rate;
    uint32_t bit_rate;
    
    // Symbol timing
    uint16_t symbol_period_us;
    uint16_t short_pulse_us;
    uint16_t long_pulse_us;
    uint8_t num_symbols;
    SymbolAlphabet_t symbols[MAX_SYMBOL_TYPES];
    
    // Frame structure
    uint16_t preamble_pattern;
    uint8_t preamble_length_bits;
    uint8_t header_length_bits;
    uint8_t payload_length_bits;
    uint8_t checksum_length_bits;
    uint32_t total_frame_bits;
    
    // Timing analysis
    uint16_t inter_frame_gap_us;
    uint16_t frame_duration_us;
    
    // Confidence scores (0-100)
    uint8_t modulation_confidence;
    uint8_t encoding_confidence;
    uint8_t timing_confidence;
    uint8_t structure_confidence;
    uint8_t overall_confidence;
    
    // Description
    char description[256];
    
} ProtocolHypothesis_t;

// Timing histogram
typedef struct {
    uint16_t bins[MAX_PULSE_BINS];
    uint16_t num_bins;
    uint16_t bin_width_us;
    uint16_t min_width_us;
    uint16_t max_width_us;
    uint16_t peak_bin;
    uint16_t peak_count;
    uint16_t total_samples;
} TimingHistogram_t;

// Pulse cluster
typedef struct {
    uint16_t center_us;
    uint16_t spread_us;
    uint16_t count;
    uint8_t assigned_symbol;
} PulseCluster_t;

// Analysis state
typedef struct {
    // Raw pulse data
    Pulse_t pulses[MAX_PULSE_COUNT];
    uint16_t pulse_count;
    
    // Timing analysis
    TimingHistogram_t mark_histogram;
    TimingHistogram_t space_histogram;
    
    // Clustering
    PulseCluster_t clusters[MAX_SYMBOL_TYPES];
    uint8_t cluster_count;
    
    // Frame samples
    Frame_t frames[MAX_FRAME_SAMPLES];
    uint16_t frame_count;
    
    // Hypothesis
    ProtocolHypothesis_t hypothesis;
    
    // State
    bool analyzing;
    uint32_t samples_collected;
    
} ProtocolInferState_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus protocol_infer_init(void);
void protocol_infer_deinit(void);

// Data collection
void protocol_infer_add_pulse(const Pulse_t* pulse);
void protocol_infer_add_frame(const Frame_t* frame);
void protocol_infer_reset(void);

// Analysis pipeline
void protocol_infer_analyze(void);
void protocol_infer_build_histograms(void);
void protocol_infer_cluster_pulses(void);
void protocol_infer_detect_modulation(void);
void protocol_infer_detect_encoding(void);
void protocol_infer_analyze_timing(void);
void protocol_infer_detect_preamble(void);
void protocol_infer_estimate_frame_structure(void);

// Hypothesis generation
void protocol_infer_generate_hypothesis(void);
const ProtocolHypothesis_t* protocol_infer_get_hypothesis(void);
uint8_t protocol_infer_get_confidence(void);

// Modulation detection
ModulationType_t protocol_infer_detect_modulation_type(const Pulse_t* pulses, 
                                                        uint16_t count);
bool protocol_infer_check_ook(const Pulse_t* pulses, uint16_t count);
bool protocol_infer_check_fsk(const Pulse_t* pulses, uint16_t count);
bool protocol_infer_check_ask(const Pulse_t* pulses, uint16_t count);

// Encoding detection
EncodingType_t protocol_infer_detect_encoding_type(const Frame_t* frames, 
                                                    uint16_t count);
bool protocol_infer_check_manchester(const Frame_t* frames, uint16_t count);
bool protocol_infer_check_miller(const Frame_t* frames, uint16_t count);
bool protocol_infer_check_pwm(const Frame_t* frames, uint16_t count);

// Timing analysis
uint32_t protocol_infer_estimate_baud_rate(void);
uint16_t protocol_infer_estimate_symbol_period(void);
void protocol_infer_calculate_timing_stats(uint16_t* min, uint16_t* max, 
                                            uint16_t* mean, uint16_t* std_dev);

// Frame structure analysis
uint16_t protocol_infer_detect_preamble_pattern(const Frame_t* frames, 
                                                 uint16_t* length_bits);
uint8_t protocol_infer_estimate_payload_length(void);
void protocol_infer_detect_checksum_type(void);

// Utility functions
void protocol_infer_print_hypothesis(const ProtocolHypothesis_t* hyp, char* buffer, 
                                      uint32_t max_len);
const char* protocol_infer_modulation_string(ModulationType_t mod);
const char* protocol_infer_encoding_string(EncodingType_t enc);

// Real-time analysis
void protocol_infer_quick_analyze(const Frame_t* frame, 
                                   ProtocolHypothesis_t* quick_result);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_INFER_H
