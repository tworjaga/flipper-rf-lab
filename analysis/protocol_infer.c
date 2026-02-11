#include "protocol_infer.h"
#include "../core/math/statistics.h"
#include <string.h>

#define TAG "PROTOCOL_INFER"

static ProtocolInferState_t infer_state;

// Initialize protocol inference
FuriStatus protocol_infer_init(void) {
    memset(&infer_state, 0, sizeof(infer_state));
    FURI_LOG_I(TAG, "Protocol inference initialized");
    return FuriStatusOk;
}

void protocol_infer_deinit(void) {
    memset(&infer_state, 0, sizeof(infer_state));
}

// Add pulse to analysis
void protocol_infer_add_pulse(const Pulse_t* pulse) {
    if(infer_state.pulse_count >= MAX_PULSE_COUNT) return;
    
    memcpy(&infer_state.pulses[infer_state.pulse_count], pulse, sizeof(Pulse_t));
    infer_state.pulse_count++;
    infer_state.samples_collected++;
}

// Add frame to analysis
void protocol_infer_add_frame(const Frame_t* frame) {
    if(infer_state.frame_count >= MAX_FRAME_SAMPLES) return;
    
    memcpy(&infer_state.frames[infer_state.frame_count], frame, sizeof(Frame_t));
    infer_state.frame_count++;
}

// Reset analysis state
void protocol_infer_reset(void) {
    infer_state.pulse_count = 0;
    infer_state.frame_count = 0;
    infer_state.cluster_count = 0;
    infer_state.samples_collected = 0;
    memset(&infer_state.mark_histogram, 0, sizeof(TimingHistogram_t));
    memset(&infer_state.space_histogram, 0, sizeof(TimingHistogram_t));
    memset(&infer_state.hypothesis, 0, sizeof(ProtocolHypothesis_t));
}

// Main analysis pipeline
void protocol_infer_analyze(void) {
    if(infer_state.pulse_count < 10 && infer_state.frame_count < 2) {
        FURI_LOG_W(TAG, "Insufficient data for analysis");
        return;
    }
    
    infer_state.analyzing = true;
    
    // Build timing histograms
    protocol_infer_build_histograms();
    
    // Cluster pulses
    protocol_infer_cluster_pulses();
    
    // Detect modulation
    protocol_infer_detect_modulation();
    
    // Detect encoding
    protocol_infer_detect_encoding();
    
    // Analyze timing
    protocol_infer_analyze_timing();
    
    // Detect preamble
    protocol_infer_detect_preamble();
    
    // Estimate frame structure
    protocol_infer_estimate_frame_structure();
    
    // Generate final hypothesis
    protocol_infer_generate_hypothesis();
    
    infer_state.analyzing = false;
    
    FURI_LOG_I(TAG, "Protocol analysis complete. Confidence: %d%%", 
               infer_state.hypothesis.overall_confidence);
}

// Build timing histograms
void protocol_infer_build_histograms(void) {
    // Find min/max pulse widths
    uint16_t min_mark = 0xFFFF, max_mark = 0;
    uint16_t min_space = 0xFFFF, max_space = 0;
    
    for(uint16_t i = 0; i < infer_state.pulse_count; i++) {
        if(infer_state.pulses[i].level == 1) {  // Mark
            if(infer_state.pulses[i].width_us < min_mark) 
                min_mark = infer_state.pulses[i].width_us;
            if(infer_state.pulses[i].width_us > max_mark) 
                max_mark = infer_state.pulses[i].width_us;
        } else {  // Space
            if(infer_state.pulses[i].width_us < min_space) 
                min_space = infer_state.pulses[i].width_us;
            if(infer_state.pulses[i].width_us > max_space) 
                max_space = infer_state.pulses[i].width_us;
        }
    }
    
    // Initialize histograms
    uint16_t mark_range = max_mark - min_mark;
    uint16_t space_range = max_space - min_space;
    
    infer_state.mark_histogram.num_bins = (mark_range < MAX_PULSE_BINS) ? 
                                          mark_range : MAX_PULSE_BINS;
    infer_state.mark_histogram.bin_width_us = (mark_range > 0) ? 
        (mark_range / infer_state.mark_histogram.num_bins) : 1;
    infer_state.mark_histogram.min_width_us = min_mark;
    infer_state.mark_histogram.max_width_us = max_mark;
    
    infer_state.space_histogram.num_bins = (space_range < MAX_PULSE_BINS) ? 
                                           space_range : MAX_PULSE_BINS;
    infer_state.space_histogram.bin_width_us = (space_range > 0) ? 
        (space_range / infer_state.space_histogram.num_bins) : 1;
    infer_state.space_histogram.min_width_us = min_space;
    infer_state.space_histogram.max_width_us = max_space;
    
    // Fill histograms
    for(uint16_t i = 0; i < infer_state.pulse_count; i++) {
        uint16_t width = infer_state.pulses[i].width_us;
        
        if(infer_state.pulses[i].level == 1) {  // Mark
            uint16_t bin = (width - min_mark) / infer_state.mark_histogram.bin_width_us;
            if(bin >= infer_state.mark_histogram.num_bins) 
                bin = infer_state.mark_histogram.num_bins - 1;
            infer_state.mark_histogram.bins[bin]++;
            infer_state.mark_histogram.total_samples++;
        } else {  // Space
            uint16_t bin = (width - min_space) / infer_state.space_histogram.bin_width_us;
            if(bin >= infer_state.space_histogram.num_bins) 
                bin = infer_state.space_histogram.num_bins - 1;
            infer_state.space_histogram.bins[bin]++;
            infer_state.space_histogram.total_samples++;
        }
    }
    
    // Find peaks
    uint16_t max_mark_count = 0, max_space_count = 0;
    for(uint16_t i = 0; i < infer_state.mark_histogram.num_bins; i++) {
        if(infer_state.mark_histogram.bins[i] > max_mark_count) {
            max_mark_count = infer_state.mark_histogram.bins[i];
            infer_state.mark_histogram.peak_bin = i;
        }
    }
    for(uint16_t i = 0; i < infer_state.space_histogram.num_bins; i++) {
        if(infer_state.space_histogram.bins[i] > max_space_count) {
            max_space_count = infer_state.space_histogram.bins[i];
            infer_state.space_histogram.peak_bin = i;
        }
    }
    
    infer_state.mark_histogram.peak_count = max_mark_count;
    infer_state.space_histogram.peak_count = max_space_count;
}

// Cluster pulses using k-means-like approach
void protocol_infer_cluster_pulses(void) {
    if(infer_state.mark_histogram.total_samples < 10) return;
    
    // Simple peak detection in histogram
    uint8_t num_clusters = 0;
    
    // Find peaks in mark histogram (up to 3 clusters)
    for(uint16_t i = 1; i < infer_state.mark_histogram.num_bins - 1 && num_clusters < 3; i++) {
        uint16_t prev = infer_state.mark_histogram.bins[i-1];
        uint16_t curr = infer_state.mark_histogram.bins[i];
        uint16_t next = infer_state.mark_histogram.bins[i+1];
        
        // Peak detection
        if(curr > prev && curr > next && curr > infer_state.mark_histogram.total_samples / 20) {
            infer_state.clusters[num_clusters].center_us = 
                infer_state.mark_histogram.min_width_us + 
                (i * infer_state.mark_histogram.bin_width_us);
            infer_state.clusters[num_clusters].count = curr;
            num_clusters++;
        }
    }
    
    infer_state.cluster_count = num_clusters;
    
    // Assign symbols
    for(uint8_t i = 0; i < num_clusters; i++) {
        infer_state.clusters[i].assigned_symbol = i;
        infer_state.clusters[i].spread_us = infer_state.mark_histogram.bin_width_us * 2;
    }
    
    FURI_LOG_I(TAG, "Found %d pulse clusters", num_clusters);
}

// Detect modulation type
void protocol_infer_detect_modulation(void) {
    infer_state.hypothesis.modulation = protocol_infer_detect_modulation_type(
        infer_state.pulses, infer_state.pulse_count);
    
    // Calculate confidence based on signal characteristics
    switch(infer_state.hypothesis.modulation) {
        case MOD_OOK:
            infer_state.hypothesis.modulation_confidence = 
                protocol_infer_check_ook(infer_state.pulses, infer_state.pulse_count) ? 90 : 50;
            break;
        case MOD_FSK:
            infer_state.hypothesis.modulation_confidence = 
                protocol_infer_check_fsk(infer_state.pulses, infer_state.pulse_count) ? 85 : 50;
            break;
        case MOD_ASK:
            infer_state.hypothesis.modulation_confidence = 
                protocol_infer_check_ask(infer_state.pulses, infer_state.pulse_count) ? 80 : 50;
            break;
        default:
            infer_state.hypothesis.modulation_confidence = 30;
    }
}

// Detect modulation from pulses
ModulationType_t protocol_infer_detect_modulation_type(const Pulse_t* pulses, 
                                                        uint16_t count) {
    if(count < 10) return MOD_UNKNOWN;
    
    // Check for OOK (On-Off Keying) - presence/absence of carrier
    uint16_t zero_count = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i].width_us > 1000) zero_count++;  // Long pulses suggest OOK
    }
    
    if(zero_count > count / 3) {
        return MOD_OOK;
    }
    
    // Check for FSK - frequency changes would be detected via RSSI or spectral analysis
    // Simplified: assume FSK if we see consistent timing patterns
    if(infer_state.cluster_count >= 2) {
        return MOD_FSK;
    }
    
    // Default to ASK for amplitude variations
    return MOD_ASK;
}

// Check OOK characteristics
bool protocol_infer_check_ook(const Pulse_t* pulses, uint16_t count) {
    // OOK has long periods of no signal
    uint32_t total_space = 0, total_mark = 0;
    uint16_t space_count = 0, mark_count = 0;
    
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i].level == 0) {
            total_space += pulses[i].width_us;
            space_count++;
        } else {
            total_mark += pulses[i].width_us;
            mark_count++;
        }
    }
    
    if(space_count == 0 || mark_count == 0) return false;
    
    uint16_t avg_space = total_space / space_count;
    uint16_t avg_mark = total_mark / mark_count;
    
    // OOK typically has asymmetric mark/space ratio
    return (avg_space > avg_mark * 2) || (avg_mark > avg_space * 2);
}

// Check FSK characteristics
bool protocol_infer_check_fsk(const Pulse_t* pulses, uint16_t count) {
    // FSK would show consistent timing with frequency changes
    // We detect this through multiple timing clusters
    return (infer_state.cluster_count >= 2);
}

// Check ASK characteristics
bool protocol_infer_check_ask(const Pulse_t* pulses, uint16_t count) {
    // ASK has amplitude variations but consistent timing
    // Simplified check
    return (infer_state.cluster_count == 1);
}

// Detect encoding type
void protocol_infer_detect_encoding(void) {
    infer_state.hypothesis.encoding = protocol_infer_detect_encoding_type(
        infer_state.frames, infer_state.frame_count);
    
    // Calculate confidence
    switch(infer_state.hypothesis.encoding) {
        case ENC_MANCHESTER:
            infer_state.hypothesis.encoding_confidence = 85;
            break;
        case ENC_PWM:
            infer_state.hypothesis.encoding_confidence = 80;
            break;
        case ENC_NRZ:
            infer_state.hypothesis.encoding_confidence = 70;
            break;
        default:
            infer_state.hypothesis.encoding_confidence = 40;
    }
}

// Detect encoding from frames
EncodingType_t protocol_infer_detect_encoding_type(const Frame_t* frames, 
                                                    uint16_t count) {
    if(count < 2) return ENC_UNKNOWN;
    
    // Check for Manchester encoding (transition in every bit period)
    if(protocol_infer_check_manchester(frames, count)) {
        return ENC_MANCHESTER;
    }
    
    // Check for PWM (Pulse Width Modulation)
    if(protocol_infer_check_pwm(frames, count)) {
        return ENC_PWM;
    }
    
    // Check for Miller encoding
    if(protocol_infer_check_miller(frames, count)) {
        return ENC_MILLER;
    }
    
    // Default to NRZ (Non-Return-to-Zero)
    return ENC_NRZ;
}

// Check Manchester encoding
bool protocol_infer_check_manchester(const Frame_t* frames, uint16_t count) {
    // Manchester has transitions in every bit cell
    // Check pulse patterns for consistent mid-bit transitions
    
    if(infer_state.pulse_count < 20) return false;
    
    uint16_t transition_count = 0;
    for(uint16_t i = 1; i < infer_state.pulse_count; i++) {
        if(infer_state.pulses[i].level != infer_state.pulses[i-1].level) {
            transition_count++;
        }
    }
    
    // Manchester should have ~50% transitions
    float transition_rate = (float)transition_count / (infer_state.pulse_count - 1);
    return (transition_rate > 0.4f && transition_rate < 0.6f);
}

// Check Miller encoding
bool protocol_infer_check_miller(const Frame_t* frames, uint16_t count) {
    // Miller has transitions at bit boundaries for 1s
    // Simplified check
    UNUSED(frames);
    UNUSED(count);
    return false;  // Would need more sophisticated analysis
}

// Check PWM encoding
bool protocol_infer_check_pwm(const Frame_t* frames, uint16_t count) {
    // PWM uses pulse width to encode data
    // Check for two distinct pulse widths
    
    if(infer_state.cluster_count < 2) return false;
    
    // Check if clusters have consistent 2:1 or 1:2 ratio
    uint16_t width1 = infer_state.clusters[0].center_us;
    uint16_t width2 = infer_state.clusters[1].center_us;
    
    float ratio = (float)width1 / (float)width2;
    return (ratio > 1.8f && ratio < 2.2f) || (ratio > 0.45f && ratio < 0.55f);
}

// Analyze timing
void protocol_infer_analyze_timing(void) {
    infer_state.hypothesis.baud_rate = protocol_infer_estimate_baud_rate();
    infer_state.hypothesis.symbol_period_us = protocol_infer_estimate_symbol_period();
    
    // Calculate confidence based on timing consistency
    uint16_t min, max, mean, std_dev;
    protocol_infer_calculate_timing_stats(&min, &max, &mean, &std_dev);
    
    // Low std_dev indicates consistent timing
    if(std_dev < mean / 10) {
        infer_state.hypothesis.timing_confidence = 90;
    } else if(std_dev < mean / 5) {
        infer_state.hypothesis.timing_confidence = 70;
    } else {
        infer_state.hypothesis.timing_confidence = 50;
    }
}

// Estimate baud rate
uint32_t protocol_infer_estimate_baud_rate(void) {
    if(infer_state.hypothesis.symbol_period_us == 0) {
        infer_state.hypothesis.symbol_period_us = protocol_infer_estimate_symbol_period();
    }
    
    if(infer_state.hypothesis.symbol_period_us == 0) return 0;
    
    // Baud rate = 1 / symbol period
    return 1000000 / infer_state.hypothesis.symbol_period_us;
}

// Estimate symbol period
uint16_t protocol_infer_estimate_symbol_period(void) {
    if(infer_state.cluster_count == 0) return 0;
    
    // Use shortest cluster center as base symbol period
    uint16_t min_period = 0xFFFF;
    for(uint8_t i = 0; i < infer_state.cluster_count; i++) {
        if(infer_state.clusters[i].center_us < min_period) {
            min_period = infer_state.clusters[i].center_us;
        }
    }
    
    return min_period;
}

// Calculate timing statistics
void protocol_infer_calculate_timing_stats(uint16_t* min, uint16_t* max, 
                                            uint16_t* mean, uint16_t* std_dev) {
    if(infer_state.pulse_count == 0) {
        *min = *max = *mean = *std_dev = 0;
        return;
    }
    
    *min = 0xFFFF;
    *max = 0;
    uint32_t sum = 0;
    
    for(uint16_t i = 0; i < infer_state.pulse_count; i++) {
        uint16_t width = infer_state.pulses[i].width_us;
        if(width < *min) *min = width;
        if(width > *max) *max = width;
        sum += width;
    }
    
    *mean = sum / infer_state.pulse_count;
    
    // Calculate standard deviation
    uint32_t variance_sum = 0;
    for(uint16_t i = 0; i < infer_state.pulse_count; i++) {
        int32_t diff = (int32_t)infer_state.pulses[i].width_us - *mean;
        variance_sum += diff * diff;
    }
    
    *std_dev = (uint16_t)sqrt(variance_sum / infer_state.pulse_count);
}

// Detect preamble pattern
void protocol_infer_detect_preamble(void) {
    uint16_t preamble_len = 0;
    infer_state.hypothesis.preamble_pattern = 
        protocol_infer_detect_preamble_pattern(infer_state.frames, 
                                                infer_state.frame_count, 
                                                &preamble_len);
    infer_state.hypothesis.preamble_length_bits = preamble_len;
}

// Detect preamble from frames
uint16_t protocol_infer_detect_preamble_pattern(const Frame_t* frames, 
                                                 uint16_t* length_bits) {
    if(infer_state.frame_count < 2) {
        *length_bits = 0;
        return 0;
    }
    
    // Look for common prefix bits
    uint8_t min_len = frames[0].length;
    for(uint16_t i = 1; i < infer_state.frame_count; i++) {
        if(frames[i].length < min_len) min_len = frames[i].length;
    }
    
    // Check bits from start
    uint8_t preamble_bytes = 0;
    for(uint8_t byte = 0; byte < min_len; byte++) {
        bool all_same = true;
        uint8_t first_val = frames[0].data[byte];
        
        for(uint16_t i = 1; i < infer_state.frame_count; i++) {
            if(frames[i].data[byte] != first_val) {
                all_same = false;
                break;
            }
        }
        
        if(all_same) {
            preamble_bytes++;
        } else {
            break;
        }
    }
    
    *length_bits = preamble_bytes * 8;
    return (preamble_bytes > 0) ? 
        ((frames[0].data[0] << 8) | (preamble_bytes > 1 ? frames[0].data[1] : 0)) : 0;
}

// Estimate payload length
uint8_t protocol_infer_estimate_payload_length(void) {
    if(infer_state.frame_count == 0) return 0;
    
    // Calculate average frame length minus preamble and checksum
    uint32_t total_len = 0;
    for(uint16_t i = 0; i < infer_state.frame_count; i++) {
        total_len += infer_state.frames[i].length;
    }
    
    uint8_t avg_len = total_len / infer_state.frame_count;
    uint8_t payload = avg_len - (infer_state.hypothesis.preamble_length_bits / 8);
    
    // Assume 1-2 bytes checksum
    if(payload > 3) payload -= 2;
    else if(payload > 2) payload -= 1;
    
    return payload;
}

// Detect checksum type
void protocol_infer_detect_checksum_type(void) {
    // Would analyze last bytes of frames to detect checksum/CRC
    // Simplified: assume last 1-2 bytes are checksum
    if(infer_state.frame_count > 0) {
        infer_state.hypothesis.checksum_length_bits = 
            (infer_state.frames[0].length > 4) ? 16 : 8;
    }
}

// Estimate frame structure
void protocol_infer_estimate_frame_structure(void) {
    infer_state.hypothesis.payload_length_bits = 
        protocol_infer_estimate_payload_length() * 8;
    infer_state.hypothesis.total_frame_bits = 
        infer_state.hypothesis.preamble_length_bits +
        infer_state.hypothesis.payload_length_bits +
        infer_state.hypothesis.checksum_length_bits;
    
    // Calculate structure confidence
    if(infer_state.frame_count >= 10) {
        infer_state.hypothesis.structure_confidence = 80;
    } else if(infer_state.frame_count >= 5) {
        infer_state.hypothesis.structure_confidence = 60;
    } else {
        infer_state.hypothesis.structure_confidence = 40;
    }
}

// Generate final hypothesis
void protocol_infer_generate_hypothesis(void) {
    ProtocolHypothesis_t* hyp = &infer_state.hypothesis;
    
    // Calculate overall confidence
    hyp->overall_confidence = 
        (hyp->modulation_confidence + 
         hyp->encoding_confidence + 
         hyp->timing_confidence + 
         hyp->structure_confidence) / 4;
    
    // Generate description
    snprintf(hyp->description, sizeof(hyp->description),
        "Protocol: %s/%s @ %lu baud\n"
        "Symbol period: %u us\n"
        "Frame: %u preamble + %u payload + %u checksum bits\n"
        "Confidence: %u%%\n",
        protocol_infer_modulation_string(hyp->modulation),
        protocol_infer_encoding_string(hyp->encoding),
        hyp->baud_rate,
        hyp->symbol_period_us,
        hyp->preamble_length_bits,
        hyp->payload_length_bits,
        hyp->checksum_length_bits,
        hyp->overall_confidence
    );
    
    // Set symbol alphabet
    hyp->num_symbols = infer_state.cluster_count;
    for(uint8_t i = 0; i < infer_state.cluster_count && i < MAX_SYMBOL_TYPES; i++) {
        hyp->symbols[i].width_us = infer_state.clusters[i].center_us;
        hyp->symbols[i].tolerance_us = infer_state.clusters[i].spread_us;
        hyp->symbols[i].symbol_value = i;
        hyp->symbols[i].name = (i == 0) ? "SHORT" : (i == 1) ? "LONG" : "SYM";
    }
}

// Get hypothesis
const ProtocolHypothesis_t* protocol_infer_get_hypothesis(void) {
    return &infer_state.hypothesis;
}

// Get confidence
uint8_t protocol_infer_get_confidence(void) {
    return infer_state.hypothesis.overall_confidence;
}

// Print hypothesis
void protocol_infer_print_hypothesis(const ProtocolHypothesis_t* hyp, char* buffer, 
                                      uint32_t max_len) {
    if(!hyp || !buffer || max_len == 0) return;
    
    strncpy(buffer, hyp->description, max_len - 1);
    buffer[max_len - 1] = '\0';
}

// Get modulation string
const char* protocol_infer_modulation_string(ModulationType_t mod) {
    switch(mod) {
        case MOD_OOK: return "OOK";
        case MOD_ASK: return "ASK";
        case MOD_FSK: return "FSK";
        case MOD_GFSK: return "GFSK";
        case MOD_MSK: return "MSK";
        case MOD_PSK: return "PSK";
        default: return "Unknown";
    }
}

// Get encoding string
const char* protocol_infer_encoding_string(EncodingType_t enc) {
    switch(enc) {
        case ENC_NRZ: return "NRZ";
        case ENC_MANCHESTER: return "Manchester";
        case ENC_MANCHESTER_IEEE: return "Manchester-IEEE";
        case ENC_MILLER: return "Miller";
        case ENC_PWM: return "PWM";
        case ENC_PPM: return "PPM";
        case ENC_RZ: return "RZ";
        default: return "Unknown";
    }
}

// Quick analysis for real-time use
void protocol_infer_quick_analyze(const Frame_t* frame, 
                                   ProtocolHypothesis_t* quick_result) {
    memset(quick_result, 0, sizeof(ProtocolHypothesis_t));
    
    // Quick modulation guess based on frame characteristics
    if(frame->rssi_dbm < -80) {
        quick_result->modulation = MOD_OOK;
        quick_result->modulation_confidence = 60;
    } else {
        quick_result->modulation = MOD_ASK;
        quick_result->modulation_confidence = 50;
    }
    
    // Estimate bit rate from frame duration
    if(frame->duration_us > 0 && frame->length > 0) {
        quick_result->bit_rate = (frame->length * 8 * 1000000) / frame->duration_us;
        quick_result->baud_rate = quick_result->bit_rate;
    }
    
    quick_result->overall_confidence = 40;  // Low confidence for quick analysis
}
