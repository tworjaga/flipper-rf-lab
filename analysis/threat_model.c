#include "threat_model.h"
#include "../core/math/fixed_point.h"
#include "../storage/sd_manager.h"
#include <string.h>
#include <math.h>

#define TAG "THREAT_MODEL"

// CRC polynomial database
static const CRCPolynomial_t crc_database[] = {
    {"CRC-8",           0x07,       8,  0x00, false, false, 0x00},
    {"CRC-8-CCITT",     0x07,       8,  0x00, false, false, 0x55},
    {"CRC-16",          0x8005,     16, 0x0000, true, true, 0x0000},
    {"CRC-16-CCITT",    0x1021,     16, 0xFFFF, true, true, 0x0000},
    {"CRC-16-IBM",      0x8005,     16, 0x0000, true, true, 0x0000},
    {"CRC-32",          0x04C11DB7, 32, 0xFFFFFFFF, true, true, 0xFFFFFFFF},
    {"CRC-32-MPEG",     0x04C11DB7, 32, 0xFFFFFFFF, false, false, 0x00000000},
};

#define NUM_CRC_POLYNOMIALS (sizeof(crc_database) / sizeof(crc_database[0]))

// Static state
static ThreatAnalysisContext_t analysis_context;
static bool threat_model_initialized = false;

// Initialize threat model
FuriStatus threat_model_init(void) {
    if(threat_model_initialized) return FuriStatusOk;
    
    FURI_LOG_I(TAG, "Initializing threat model");
    memset(&analysis_context, 0, sizeof(analysis_context));
    threat_model_initialized = true;
    
    return FuriStatusOk;
}

void threat_model_deinit(void) {
    threat_model_initialized = false;
}

// Start analysis
void threat_model_start_analysis(void) {
    memset(&analysis_context, 0, sizeof(analysis_context));
    analysis_context.state = THREAT_STATE_COLLECTING;
    analysis_context.analysis_start_time = furi_get_tick();
    FURI_LOG_I(TAG, "Started threat analysis");
}

void threat_model_stop_analysis(void) {
    analysis_context.state = THREAT_STATE_IDLE;
}

bool threat_model_is_analyzing(void) {
    return (analysis_context.state != THREAT_STATE_IDLE);
}

// Add frame to analysis
void threat_model_add_frame(const Frame_t* frame) {
    if(analysis_context.frame_count >= MAX_FRAME_SAMPLES) return;
    
    uint16_t idx = analysis_context.frame_count;
    uint8_t len = (frame->length < MAX_PAYLOAD_SIZE) ? frame->length : MAX_PAYLOAD_SIZE;
    
    memcpy(analysis_context.payloads[idx], frame->data, len);
    analysis_context.payload_lengths[idx] = len;
    analysis_context.frame_count++;
    
    // Update byte frequencies
    threat_model_update_byte_frequencies(frame->data, len);
}

// Add raw payload
void threat_model_add_payload(const uint8_t* data, uint8_t len) {
    if(analysis_context.frame_count >= MAX_FRAME_SAMPLES) return;
    
    uint16_t idx = analysis_context.frame_count;
    uint8_t copy_len = (len < MAX_PAYLOAD_SIZE) ? len : MAX_PAYLOAD_SIZE;
    
    memcpy(analysis_context.payloads[idx], data, copy_len);
    analysis_context.payload_lengths[idx] = copy_len;
    analysis_context.frame_count++;
    
    threat_model_update_byte_frequencies(data, len);
}

// Update byte frequency counts
void threat_model_update_byte_frequencies(const uint8_t* data, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        analysis_context.byte_frequencies[data[i]]++;
        analysis_context.total_bytes++;
    }
}

// Calculate Shannon entropy
float threat_model_calculate_entropy(void) {
    if(analysis_context.total_bytes == 0) return 0.0f;
    
    float entropy = 0.0f;
    
    for(uint16_t i = 0; i < 256; i++) {
        if(analysis_context.byte_frequencies[i] > 0) {
            float probability = (float)analysis_context.byte_frequencies[i] / 
                               analysis_context.total_bytes;
            entropy -= probability * log2f(probability);
        }
    }
    
    analysis_context.entropy_per_byte = entropy;
    return entropy;
}

// Calculate entropy for specific data
float threat_model_calculate_byte_entropy(const uint8_t* data, uint8_t len) {
    uint32_t frequencies[256] = {0};
    
    for(uint8_t i = 0; i < len; i++) {
        frequencies[data[i]]++;
    }
    
    float entropy = 0.0f;
    for(uint16_t i = 0; i < 256; i++) {
        if(frequencies[i] > 0) {
            float probability = (float)frequencies[i] / len;
            entropy -= probability * log2f(probability);
        }
    }
    
    return entropy;
}

// Detect static (unchanging) patterns
void threat_model_detect_static_patterns(void) {
    if(analysis_context.frame_count < 2) return;
    
    uint8_t min_len = analysis_context.payload_lengths[0];
    for(uint16_t i = 1; i < analysis_context.frame_count; i++) {
        if(analysis_context.payload_lengths[i] < min_len) {
            min_len = analysis_context.payload_lengths[i];
        }
    }
    
    // For each bit position, check if it's constant across all frames
    memset(analysis_context.static_bit_mask, 0xFF, sizeof(analysis_context.static_bit_mask));
    
    for(uint16_t frame = 1; frame < analysis_context.frame_count; frame++) {
        for(uint8_t byte = 0; byte < min_len; byte++) {
            uint8_t diff = analysis_context.payloads[0][byte] ^ 
                          analysis_context.payloads[frame][byte];
            
            // Clear bits that differ
            uint32_t word_idx = byte / 4;
            uint8_t bit_offset = (byte % 4) * 8;
            analysis_context.static_bit_mask[word_idx] &= ~(diff << bit_offset);
        }
    }
    
    // Calculate static ratio
    uint16_t static_bits = 0;
    uint16_t total_bits = min_len * 8;
    
    for(uint8_t i = 0; i < min_len; i++) {
        uint32_t word_idx = i / 4;
        uint8_t bit_offset = (i % 4) * 8;
        uint8_t static_byte = (analysis_context.static_bit_mask[word_idx] >> bit_offset) & 0xFF;
        
        // Count bits set in static_byte
        for(uint8_t b = 0; b < 8; b++) {
            if(static_byte & (1 << b)) static_bits++;
        }
    }
    
    analysis_context.static_ratio = (static_bits * 100) / total_bits;
}

// Calculate static ratio percentage
uint8_t threat_model_calculate_static_ratio(void) {
    return analysis_context.static_ratio;
}

// Detect preamble pattern
void threat_model_detect_preamble(void) {
    if(analysis_context.frame_count < 2) return;
    
    // Look for common prefix across frames
    uint8_t max_preamble_len = 4;  // Max bytes to check
    
    for(uint8_t len = 1; len <= max_preamble_len; len++) {
        bool match = true;
        for(uint16_t i = 1; i < analysis_context.frame_count; i++) {
            if(memcmp(analysis_context.payloads[0], 
                     analysis_context.payloads[i], len) != 0) {
                match = false;
                break;
            }
        }
        
        if(match) {
            analysis_context.preamble_length = len;
            analysis_context.fixed_preamble = 0;
            for(uint8_t i = 0; i < len; i++) {
                analysis_context.fixed_preamble = (analysis_context.fixed_preamble << 8) | 
                                                analysis_context.payloads[0][i];
            }
        } else {
            break;
        }
    }
}

// Analyze CRC
void threat_model_analyze_crc(void) {
    analysis_context.state = THREAT_STATE_ANALYZING_CRC;
    
    if(analysis_context.frame_count < 5) return;
    
    // Try each CRC polynomial
    for(uint8_t p = 0; p < NUM_CRC_POLYNOMIALS; p++) {
        const CRCPolynomial_t* poly = &crc_database[p];
        uint8_t match_count = 0;
        
        for(uint16_t i = 0; i < analysis_context.frame_count; i++) {
            uint8_t len = analysis_context.payload_lengths[i];
            if(len < 3) continue;  // Need at least data + CRC
            
            // Try different CRC positions
            for(uint8_t crc_pos = len - 2; crc_pos >= len - 4 && crc_pos > 0; crc_pos--) {
                if(threat_model_test_crc(analysis_context.payloads[i], crc_pos, poly)) {
                    match_count++;
                    break;
                }
            }
        }
        
        // If >80% of frames match, we found the CRC
        if(match_count > (analysis_context.frame_count * 8 / 10)) {
            analysis_context.suspected_crc_type = p;
            analysis_context.suspected_polynomial = poly->polynomial;
            analysis_context.crc_validated = true;
            
            FURI_LOG_I(TAG, "Detected CRC: %s", poly->name);
            break;
        }
    }
}

// Test if CRC matches
bool threat_model_test_crc(const uint8_t* data, uint8_t len, 
                           const CRCPolynomial_t* poly) {
    if(poly->width == 8) {
        uint8_t crc = threat_model_calculate_crc8(data, len, 
                                                    poly->polynomial & 0xFF, 
                                                    poly->initial);
        return (crc == data[len]);
    } else if(poly->width == 16) {
        uint16_t crc = threat_model_calculate_crc16(data, len, 
                                                     poly->polynomial, 
                                                     poly->initial);
        return ((crc >> 8) == data[len] && (crc & 0xFF) == data[len + 1]);
    }
    return false;
}

// Calculate CRC-16
uint16_t threat_model_calculate_crc16(const uint8_t* data, uint8_t len,
                                       uint16_t polynomial, uint16_t init) {
    uint16_t crc = init;
    
    for(uint8_t i = 0; i < len; i++) {
        crc ^= (data[i] << 8);
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// Calculate CRC-8
uint8_t threat_model_calculate_crc8(const uint8_t* data, uint8_t len,
                                     uint8_t polynomial, uint8_t init) {
    uint8_t crc = init;
    
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// Detect rolling code
void threat_model_detect_rolling_code(void) {
    if(analysis_context.frame_count < ENTROPY_HISTORY_SIZE) return;
    
    // Look for fields that change in every frame but aren't random
    // This is a simplified check - full implementation would be more sophisticated
    
    uint8_t min_len = analysis_context.payload_lengths[0];
    
    for(uint8_t byte_pos = 0; byte_pos < min_len; byte_pos++) {
        uint32_t values[ENTROPY_HISTORY_SIZE];
        uint8_t value_count = 0;
        
        for(uint16_t i = 0; i < analysis_context.frame_count && i < ENTROPY_HISTORY_SIZE; i++) {
            // Extract 4-byte value at this position
            if(byte_pos + 4 <= analysis_context.payload_lengths[i]) {
                uint32_t val = 0;
                for(uint8_t b = 0; b < 4; b++) {
                    val = (val << 8) | analysis_context.payloads[i][byte_pos + b];
                }
                values[value_count++] = val;
            }
        }
        
        // Check if values are sequential or have pattern
        if(value_count >= 10) {
            bool sequential = true;
            for(uint8_t i = 1; i < value_count; i++) {
                if(values[i] != values[i-1] + 1 && 
                   values[i] != values[i-1] - 1 &&
                   values[i] != values[i-1]) {
                    sequential = false;
                    break;
                }
            }
            
            if(!sequential && threat_model_analyze_sequence_randomness(values, value_count)) {
                analysis_context.rolling_code_detected = true;
                analysis_context.rolling_code_field_position = byte_pos;
                analysis_context.rolling_code_field_length = 4;
                
                // Store sequence for analysis
                memcpy(analysis_context.rolling_code_sequence, values, 
                       value_count * sizeof(uint32_t));
                
                FURI_LOG_I(TAG, "Rolling code detected at byte %d", byte_pos);
                break;
            }
        }
    }
}

// Analyze sequence randomness
bool threat_model_analyze_sequence_randomness(const uint32_t* sequence, uint8_t len) {
    // Simple randomness test: check for patterns
    // Full implementation would use more sophisticated tests
    
    // Check for repeating patterns
    for(uint8_t period = 1; period <= len / 2; period++) {
        bool repeating = true;
        for(uint8_t i = period; i < len; i++) {
            if(sequence[i] != sequence[i % period]) {
                repeating = false;
                break;
            }
        }
        if(repeating) return false;  // Not random if repeating
    }
    
    return true;
}

// Estimate entropy bits in data
uint8_t threat_model_estimate_entropy_bits(const uint8_t* data, uint8_t len) {
    float entropy = threat_model_calculate_byte_entropy(data, len);
    return (uint8_t)(entropy * len);  // Total entropy in bits
}

// Detect replay vulnerability
void threat_model_detect_replay_vulnerability(void) {
    analysis_context.exact_replay_detected = false;
    analysis_context.replay_count = 0;
    
    // Check for identical frames
    for(uint16_t i = 0; i < analysis_context.frame_count; i++) {
        for(uint16_t j = i + 1; j < analysis_context.frame_count; j++) {
            if(analysis_context.payload_lengths[i] == analysis_context.payload_lengths[j]) {
                if(memcmp(analysis_context.payloads[i], 
                         analysis_context.payloads[j],
                         analysis_context.payload_lengths[i]) == 0) {
                    analysis_context.exact_replay_detected = true;
                    if(analysis_context.replay_count < 10) {
                        analysis_context.replay_frame_indices[analysis_context.replay_count++] = i;
                    }
                }
            }
        }
    }
}

// Check frame uniqueness
bool threat_model_check_frame_uniqueness(const uint8_t* data, uint8_t len) {
    for(uint16_t i = 0; i < analysis_context.frame_count; i++) {
        if(analysis_context.payload_lengths[i] == len) {
            if(memcmp(analysis_context.payloads[i], data, len) == 0) {
                return false;  // Not unique
            }
        }
    }
    return true;
}

// Assess vulnerabilities
void threat_model_assess_vulnerabilities(void) {
    analysis_context.state = THREAT_STATE_ASSESSING;
    
    // Run all analyses
    threat_model_calculate_entropy();
    threat_model_detect_static_patterns();
    threat_model_detect_preamble();
    threat_model_analyze_crc();
    threat_model_detect_rolling_code();
    threat_model_detect_replay_vulnerability();
    
    // Calculate risk level
    analysis_context.assessment.level = threat_model_calculate_risk_level();
    analysis_context.assessment.vulnerability_score = 
        threat_model_calculate_vulnerability_score();
    analysis_context.assessment.entropy_bits = 
        (uint8_t)(analysis_context.entropy_per_byte * 8);
    analysis_context.assessment.has_checksum = analysis_context.crc_validated;
    analysis_context.assessment.has_rolling_code = analysis_context.rolling_code_detected;
    analysis_context.assessment.is_static = (analysis_context.static_ratio > 80);
    analysis_context.assessment.static_ratio = analysis_context.static_ratio;
    
    // Generate description
    threat_model_generate_report();
    
    analysis_context.state = THREAT_STATE_COMPLETE;
    
    FURI_LOG_I(TAG, "Threat assessment complete: %s (score: %d)",
               threat_model_get_risk_string(analysis_context.assessment.level),
               analysis_context.assessment.vulnerability_score);
}

// Calculate risk level
RiskLevel_t threat_model_calculate_risk_level(void) {
    uint16_t score = threat_model_calculate_vulnerability_score();
    
    if(score >= VULN_SCORE_CRITICAL) return RISK_CRITICAL;
    if(score >= VULN_SCORE_HIGH) return RISK_HIGH;
    if(score >= VULN_SCORE_MEDIUM) return RISK_MEDIUM;
    if(score >= VULN_SCORE_LOW) return RISK_LOW;
    return RISK_LOW;
}

// Calculate vulnerability score (0-1000)
uint16_t threat_model_calculate_vulnerability_score(void) {
    uint16_t score = 0;
    
    // Low entropy: up to 300 points
    if(analysis_context.entropy_per_byte < 4.0f) {
        score += (uint16_t)((4.0f - analysis_context.entropy_per_byte) * 75);
    }
    
    // Static frames: up to 250 points
    score += (analysis_context.static_ratio * 250) / 100;
    
    // No CRC: 200 points
    if(!analysis_context.crc_validated) {
        score += 200;
    }
    
    // No rolling code: 150 points
    if(!analysis_context.rolling_code_detected) {
        score += 150;
    }
    
    // Replay vulnerability: 100 points
    if(analysis_context.exact_replay_detected) {
        score += 100;
    }
    
    return (score > 1000) ? 1000 : score;
}

// Generate detailed report
void threat_model_generate_report(void) {
    snprintf(analysis_context.detailed_report, sizeof(analysis_context.detailed_report),
        "RF THREAT ANALYSIS REPORT\n"
        "========================\n\n"
        "Risk Level: %s\n"
        "Vulnerability Score: %d/1000\n\n"
        "ENTROPY ANALYSIS:\n"
        "  Entropy per byte: %.2f bits\n"
        "  Total entropy: %d bits\n\n"
        "PATTERN ANALYSIS:\n"
        "  Static ratio: %d%%\n"
        "  Preamble length: %d bytes\n"
        "  Fixed preamble: 0x%04X\n\n"
        "SECURITY FEATURES:\n"
        "  Checksum/CRC: %s\n"
        "  Rolling code: %s\n"
        "  Replay vulnerable: %s\n\n"
        "RECOMMENDATION:\n"
        "  %s\n",
        threat_model_get_risk_string(analysis_context.assessment.level),
        analysis_context.assessment.vulnerability_score,
        analysis_context.entropy_per_byte,
        analysis_context.assessment.entropy_bits,
        analysis_context.static_ratio,
        analysis_context.preamble_length,
        analysis_context.fixed_preamble,
        analysis_context.crc_validated ? "YES" : "NO",
        analysis_context.rolling_code_detected ? "YES" : "NO",
        analysis_context.exact_replay_detected ? "YES" : "NO",
        analysis_context.assessment.level == RISK_CRITICAL ? 
            "CRITICAL: Device is highly vulnerable to replay attacks" :
        analysis_context.assessment.level == RISK_HIGH ?
            "HIGH: Implement rolling code or encryption immediately" :
        analysis_context.assessment.level == RISK_MEDIUM ?
            "MEDIUM: Consider adding authentication mechanisms" :
            "LOW: Device has basic security measures in place"
    );
}

// Get report string
const char* threat_model_get_report(void) {
    return analysis_context.detailed_report;
}

// Get risk level string
const char* threat_model_get_risk_string(RiskLevel_t risk) {
    switch(risk) {
        case RISK_LOW: return "LOW";
        case RISK_MEDIUM: return "MEDIUM";
        case RISK_HIGH: return "HIGH";
        case RISK_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Calculate Hamming distance
uint8_t threat_model_hamming_distance(const uint8_t* a, const uint8_t* b, uint8_t len) {
    uint8_t distance = 0;
    for(uint8_t i = 0; i < len; i++) {
        uint8_t diff = a[i] ^ b[i];
        // Count bits
        for(uint8_t j = 0; j < 8; j++) {
            if(diff & (1 << j)) distance++;
        }
    }
    return distance;
}

// Bitwise XOR
void threat_model_bitwise_xor(const uint8_t* a, const uint8_t* b,
                               uint8_t* result, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        result[i] = a[i] ^ b[i];
    }
}

// Export report to file
bool threat_model_export_report(const char* filename) {
    return sd_manager_export_report(&analysis_context.assessment, filename);
}

// Quick assessment for real-time analysis
void threat_model_quick_assess(const Frame_t* frame, ThreatAssessment_t* quick_result) {
    // Simplified assessment based on single frame
    memset(quick_result, 0, sizeof(ThreatAssessment_t));
    
    // Calculate entropy
    float entropy = threat_model_calculate_byte_entropy(frame->data, frame->length);
    quick_result->entropy_bits = (uint8_t)(entropy * frame->length);
    
    // Check for static patterns (all same byte)
    bool all_same = true;
    for(uint8_t i = 1; i < frame->length; i++) {
        if(frame->data[i] != frame->data[0]) {
            all_same = false;
            break;
        }
    }
    
    if(all_same) {
        quick_result->is_static = true;
        quick_result->static_ratio = 100;
    }
    
    // Estimate risk
    if(entropy < 2.0f || all_same) {
        quick_result->level = RISK_HIGH;
        quick_result->vulnerability_score = 700;
    } else if(entropy < 4.0f) {
        quick_result->level = RISK_MEDIUM;
        quick_result->vulnerability_score = 400;
    } else {
        quick_result->level = RISK_LOW;
        quick_result->vulnerability_score = 200;
    }
}

// Get CRC database
const CRCPolynomial_t* threat_model_get_crc_database(void) {
    return crc_database;
}

// Count CRC polynomials
uint8_t threat_model_count_crc_polymorphs(void) {
    return NUM_CRC_POLYNOMIALS;
}

// Verify checksum at position
bool threat_model_verify_checksum(const uint8_t* data, uint8_t len, 
                                   uint8_t checksum_pos) {
    if(checksum_pos >= len) return false;
    
    // Simple sum checksum
    uint8_t sum = 0;
    for(uint8_t i = 0; i < checksum_pos; i++) {
        sum += data[i];
    }
    
    return (sum == data[checksum_pos]);
}

// Find fixed fields
bool threat_model_find_fixed_fields(uint8_t* field_positions, uint8_t* field_lengths, 
                                     uint8_t* num_fields) {
    if(analysis_context.frame_count < 2) return false;
    
    *num_fields = 0;
    uint8_t min_len = analysis_context.payload_lengths[0];
    
    // Find runs of static bits
    bool in_field = false;
    uint8_t field_start = 0;
    
    for(uint8_t byte = 0; byte < min_len && *num_fields < 8; byte++) {
        uint32_t word_idx = byte / 4;
        uint8_t bit_offset = (byte % 4) * 8;
        uint8_t static_byte = (analysis_context.static_bit_mask[word_idx] >> bit_offset) & 0xFF;
        
        if(static_byte == 0xFF) {
            // All bits static in this byte
            if(!in_field) {
                field_start = byte;
                in_field = true;
            }
        } else {
            if(in_field) {
                // End of field
                field_positions[*num_fields] = field_start;
                field_lengths[*num_fields] = byte - field_start;
                (*num_fields)++;
                in_field = false;
            }
        }
    }
    
    // Close final field if needed
    if(in_field && *num_fields < 8) {
        field_positions[*num_fields] = field_start;
        field_lengths[*num_fields] = min_len - field_start;
        (*num_fields)++;
    }
    
    return (*num_fields > 0);
}
