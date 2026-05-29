#ifndef THREAT_MODEL_H
#define THREAT_MODEL_H

#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// RF THREAT MODELING ENGINE
// Pentest-grade vulnerability analysis
// ============================================================================

#define MAX_PAYLOAD_SIZE        64
#define MAX_FRAME_SAMPLES       256
#define ENTROPY_HISTORY_SIZE    100
#define CRC_POLYNOMIALS_COUNT   10

// Vulnerability scoring thresholds
#define VULN_SCORE_CRITICAL     900     // 90.0+
#define VULN_SCORE_HIGH         700     // 70.0-89.9
#define VULN_SCORE_MEDIUM       400     // 40.0-69.9
#define VULN_SCORE_LOW          200     // 20.0-39.9
#define VULN_SCORE_NONE         0       // 0-19.9

// ============================================================================
// ANALYSIS STATE
// ============================================================================

typedef enum {
    THREAT_STATE_IDLE = 0,
    THREAT_STATE_COLLECTING,
    THREAT_STATE_ANALYZING_ENTROPY,
    THREAT_STATE_ANALYZING_PATTERNS,
    THREAT_STATE_ANALYZING_CRC,
    THREAT_STATE_ASSESSING,
    THREAT_STATE_COMPLETE
} ThreatAnalysisState_t;

typedef struct {
    // Frame collection
    uint8_t payloads[MAX_FRAME_SAMPLES][MAX_PAYLOAD_SIZE];
    uint8_t payload_lengths[MAX_FRAME_SAMPLES];
    uint16_t frame_count;
    
    // Entropy analysis
    uint8_t byte_frequencies[256];
    uint32_t total_bytes;
    float entropy_per_byte;
    uint8_t entropy_histogram[ENTROPY_HISTORY_SIZE];
    
    // Pattern detection
    uint32_t static_bit_mask[MAX_PAYLOAD_SIZE / 32 + 1];
    uint8_t static_ratio;
    uint16_t fixed_preamble;
    uint8_t preamble_length;
    
    // CRC analysis
    uint8_t suspected_crc_type;
    uint16_t suspected_polynomial;
    uint8_t crc_position;
    bool crc_validated;
    
    // Rolling code detection
    bool rolling_code_detected;
    uint8_t rolling_code_field_position;
    uint8_t rolling_code_field_length;
    uint32_t rolling_code_sequence[ENTROPY_HISTORY_SIZE];
    
    // Replay detection
    bool exact_replay_detected;
    uint16_t replay_frame_indices[10];
    uint8_t replay_count;
    
    // Assessment
    ThreatAssessment_t assessment;
    char detailed_report[512];
    
    // State
    ThreatAnalysisState_t state;
    uint32_t analysis_start_time;
    
} ThreatAnalysisContext_t;

// ============================================================================
// CRC POLYNOMIAL DATABASE
// ============================================================================

typedef struct {
    const char* name;
    uint16_t polynomial;
    uint8_t width;        // 8, 16, or 32
    uint8_t initial;
    bool reflect_in;
    bool reflect_out;
    uint16_t xor_out;
} CRCPolynomial_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus threat_model_init(void);
void threat_model_deinit(void);

// Analysis control
void threat_model_start_analysis(void);
void threat_model_stop_analysis(void);
bool threat_model_is_analyzing(void);

// Frame collection
void threat_model_add_frame(const Frame_t* frame);
void threat_model_add_payload(const uint8_t* data, uint8_t len);

// Entropy analysis
float threat_model_calculate_entropy(void);
float threat_model_calculate_byte_entropy(const uint8_t* data, uint8_t len);
void threat_model_update_byte_frequencies(const uint8_t* data, uint8_t len);

// Pattern detection
void threat_model_detect_static_patterns(void);
void threat_model_detect_preamble(void);
uint8_t threat_model_calculate_static_ratio(void);
bool threat_model_find_fixed_fields(uint8_t* field_positions, uint8_t* field_lengths, 
                                     uint8_t* num_fields);

// CRC analysis
void threat_model_analyze_crc(void);
bool threat_model_test_crc(const uint8_t* data, uint8_t len, 
                           const CRCPolynomial_t* poly);
const CRCPolynomial_t* threat_model_get_crc_database(void);
uint8_t threat_model_count_crc_polymorphs(void);

// Rolling code detection
void threat_model_detect_rolling_code(void);
bool threat_model_analyze_sequence_randomness(const uint32_t* sequence, uint8_t len);
uint8_t threat_model_estimate_entropy_bits(const uint8_t* data, uint8_t len);

// Replay attack detection
void threat_model_detect_replay_vulnerability(void);
bool threat_model_check_frame_uniqueness(const uint8_t* data, uint8_t len);

// Vulnerability assessment
void threat_model_assess_vulnerabilities(void);
RiskLevel_t threat_model_calculate_risk_level(void);
uint16_t threat_model_calculate_vulnerability_score(void);

// Report generation
void threat_model_generate_report(void);
const char* threat_model_get_report(void);
const char* threat_model_get_risk_string(RiskLevel_t risk);

// Utility functions
uint16_t threat_model_calculate_crc16(const uint8_t* data, uint8_t len, 
                                       uint16_t polynomial, uint16_t init);
uint8_t threat_model_calculate_crc8(const uint8_t* data, uint8_t len, 
                                     uint8_t polynomial, uint8_t init);
bool threat_model_verify_checksum(const uint8_t* data, uint8_t len, 
                                   uint8_t checksum_pos);

// Bit-level analysis
uint8_t threat_model_hamming_distance(const uint8_t* a, const uint8_t* b, uint8_t len);
void threat_model_bitwise_xor(const uint8_t* a, const uint8_t* b, 
                               uint8_t* result, uint8_t len);

// Export
bool threat_model_export_report(const char* filename);

// Real-time analysis
void threat_model_quick_assess(const Frame_t* frame, ThreatAssessment_t* quick_result);

#ifdef __cplusplus
}
#endif

#endif // THREAT_MODEL_H
