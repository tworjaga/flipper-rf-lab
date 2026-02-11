#include "compression.h"
#include <string.h>

#define TAG "COMPRESSION"

// Static state
static HuffmanState_t huffman_state;
static RLEState_t rle_state;
static DeltaState_t delta_state;
static bool compression_initialized = false;

// Initialize compression engine
void compression_init(void) {
    if(compression_initialized) return;
    
    memset(&huffman_state, 0, sizeof(huffman_state));
    memset(&rle_state, 0, sizeof(rle_state));
    memset(&delta_state, 0, sizeof(delta_state));
    
    compression_initialized = true;
}

void compression_deinit(void) {
    compression_initialized = false;
}

// High-level compression function
bool compress_data(const uint8_t* input, uint32_t input_len,
                   uint8_t* output, uint32_t* output_len,
                   CompressionAlgorithm_t algorithm,
                   CompressionStats_t* stats) {
    if(!compression_initialized || !input || !output || !output_len) {
        return false;
    }
    
    uint32_t start_time = 0;  // Would use timer_get_us()
    uint32_t compressed_size = 0;
    
    switch(algorithm) {
        case COMPRESS_DELTA:
            compressed_size = delta_encode(input, input_len, output);
            break;
            
        case COMPRESS_RLE:
            compressed_size = rle_encode(input, input_len, output);
            break;
            
        case COMPRESS_HUFFMAN:
            huffman_init(&huffman_state);
            huffman_build_tree(&huffman_state, input, input_len);
            huffman_generate_codes(&huffman_state);
            compressed_size = huffman_encode(&huffman_state, input, input_len, output);
            break;
            
        case COMPRESS_LZ77:
            compressed_size = lz77_encode(input, input_len, output, 4096, 18);
            break;
            
        case COMPRESS_ADAPTIVE:
            // Try multiple algorithms and pick best
            algorithm = compression_select_algorithm(input, input_len);
            return compress_data(input, input_len, output, output_len, 
                                algorithm, stats);
            
        case COMPRESS_NONE:
        default:
            memcpy(output, input, input_len);
            compressed_size = input_len;
            break;
    }
    
    *output_len = compressed_size;
    
    if(stats) {
        stats->original_size = input_len;
        stats->compressed_size = compressed_size;
        stats->ratio = (float)input_len / (float)compressed_size;
        stats->algorithm = algorithm;
        stats->encode_time_us = 0;  // Would calculate
    }
    
    return true;
}

// High-level decompression
bool decompress_data(const uint8_t* input, uint32_t input_len,
                     uint8_t* output, uint32_t* output_len,
                     CompressionStats_t* stats) {
    // Would implement based on header indicating algorithm
    // For now, just copy
    memcpy(output, input, input_len);
    *output_len = input_len;
    return true;
}

// Delta encoding
uint32_t delta_encode(const uint8_t* input, uint32_t len, uint8_t* output) {
    if(len == 0) return 0;
    
    uint32_t out_pos = 0;
    int16_t last = input[0];
    
    // Store first value as-is
    output[out_pos++] = input[0];
    
    // Store deltas
    for(uint32_t i = 1; i < len && out_pos < COMPRESSION_MAX_BLOCK_SIZE; i++) {
        int16_t delta = (int16_t)input[i] - last;
        
        // Encode delta as signed byte if possible
        if(delta >= -128 && delta <= 127) {
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        } else {
            // Escape code for large delta
            output[out_pos++] = 0x80;  // Escape
            output[out_pos++] = (uint8_t)(delta >> 8);
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        }
        
        last = input[i];
    }
    
    return out_pos;
}

// Delta decoding
uint32_t delta_decode(const uint8_t* input, uint32_t len, uint8_t* output) {
    if(len == 0) return 0;
    
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    int16_t last = input[in_pos++];
    output[out_pos++] = (uint8_t)last;
    
    while(in_pos < len) {
        uint8_t byte = input[in_pos++];
        
        if(byte == 0x80 && in_pos + 1 < len) {
            // Large delta
            int16_t delta = (int16_t)((input[in_pos] << 8) | input[in_pos + 1]);
            in_pos += 2;
            last += delta;
        } else {
            // Small delta (sign-extended)
            int16_t delta = (int8_t)byte;
            last += delta;
        }
        
        output[out_pos++] = (uint8_t)last;
    }
    
    return out_pos;
}

// 16-bit delta encoding
uint32_t delta_encode_16bit(const uint16_t* input, uint32_t len, uint8_t* output) {
    if(len == 0) return 0;
    
    uint32_t out_pos = 0;
    int32_t last = input[0];
    
    // Store first value (2 bytes)
    output[out_pos++] = input[0] >> 8;
    output[out_pos++] = input[0] & 0xFF;
    
    for(uint32_t i = 1; i < len && out_pos + 1 < COMPRESSION_MAX_BLOCK_SIZE; i++) {
        int32_t delta = (int32_t)input[i] - last;
        
        // Encode as variable length
        if(delta >= -128 && delta <= 127) {
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        } else if(delta >= -32768 && delta <= 32767) {
            output[out_pos++] = 0x80;
            output[out_pos++] = (uint8_t)(delta >> 8);
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        } else {
            output[out_pos++] = 0x81;
            output[out_pos++] = (uint8_t)(delta >> 24);
            output[out_pos++] = (uint8_t)(delta >> 16);
            output[out_pos++] = (uint8_t)(delta >> 8);
            output[out_pos++] = (uint8_t)(delta & 0xFF);
        }
        
        last = input[i];
    }
    
    return out_pos;
}

// 16-bit delta decoding
uint32_t delta_decode_16bit(const uint8_t* input, uint32_t len, uint16_t* output) {
    if(len < 2) return 0;
    
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    int32_t last = ((uint16_t)input[in_pos] << 8) | input[in_pos + 1];
    in_pos += 2;
    output[out_pos++] = (uint16_t)last;
    
    while(in_pos < len) {
        uint8_t byte = input[in_pos++];
        int32_t delta;
        
        if(byte == 0x80 && in_pos + 1 < len) {
            delta = (int16_t)((input[in_pos] << 8) | input[in_pos + 1]);
            in_pos += 2;
        } else if(byte == 0x81 && in_pos + 3 < len) {
            delta = (input[in_pos] << 24) | (input[in_pos + 1] << 16) |
                   (input[in_pos + 2] << 8) | input[in_pos + 3];
            in_pos += 4;
        } else {
            delta = (int8_t)byte;
        }
        
        last += delta;
        output[out_pos++] = (uint16_t)last;
    }
    
    return out_pos;
}

// RLE encoding
uint32_t rle_encode(const uint8_t* input, uint32_t len, uint8_t* output) {
    if(len == 0) return 0;
    
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    
    while(in_pos < len && out_pos < COMPRESSION_MAX_BLOCK_SIZE - 2) {
        uint8_t symbol = input[in_pos];
        uint8_t run_length = 1;
        
        // Count run length
        while(in_pos + run_length < len && 
              input[in_pos + run_length] == symbol && 
              run_length < RLE_MAX_RUN_LENGTH) {
            run_length++;
        }
        
        if(run_length >= 3) {
            // Encode as run
            output[out_pos++] = 0x00;  // Run indicator
            output[out_pos++] = run_length;
            output[out_pos++] = symbol;
            in_pos += run_length;
        } else {
            // Encode literal (escape if needed)
            if(symbol == 0x00) {
                output[out_pos++] = 0x00;
                output[out_pos++] = 0x01;  // Escape
            }
            output[out_pos++] = symbol;
            in_pos++;
        }
    }
    
    return out_pos;
}

// RLE decoding
uint32_t rle_decode(const uint8_t* input, uint32_t len, uint8_t* output) {
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    
    while(in_pos < len) {
        uint8_t byte = input[in_pos++];
        
        if(byte == 0x00) {
            if(in_pos >= len) break;
            
            uint8_t next = input[in_pos++];
            
            if(next == 0x00) {
                // Literal 0x00
                output[out_pos++] = 0x00;
            } else if(next == 0x01) {
                // Escaped literal
                if(in_pos >= len) break;
                output[out_pos++] = input[in_pos++];
            } else {
                // Run
                uint8_t run_length = next;
                if(in_pos >= len) break;
                uint8_t symbol = input[in_pos++];
                
                for(uint8_t i = 0; i < run_length && out_pos < COMPRESSION_MAX_BLOCK_SIZE; i++) {
                    output[out_pos++] = symbol;
                }
            }
        } else {
            output[out_pos++] = byte;
        }
    }
    
    return out_pos;
}

// Adaptive RLE
uint32_t rle_encode_adaptive(const uint8_t* input, uint32_t len, uint8_t* output) {
    // Analyze data to choose RLE threshold
    uint32_t run_count = 0;
    for(uint32_t i = 1; i < len; i++) {
        if(input[i] == input[i-1]) run_count++;
    }
    
    // If many runs, use RLE, otherwise use delta
    if(run_count > len / 4) {
        return rle_encode(input, len, output);
    } else {
        return delta_encode(input, len, output);
    }
}

// Initialize Huffman state
void huffman_init(HuffmanState_t* state) {
    memset(state, 0, sizeof(HuffmanState_t));
    state->initialized = true;
}

// Build Huffman tree
void huffman_build_tree(HuffmanState_t* state, const uint8_t* data, uint32_t len) {
    if(!state->initialized) return;
    
    // Count frequencies
    memset(state->frequencies, 0, sizeof(state->frequencies));
    for(uint32_t i = 0; i < len; i++) {
        state->frequencies[data[i]]++;
    }
    
    // Initialize leaf nodes
    state->num_nodes = 0;
    for(uint16_t i = 0; i < COMPRESSION_MAX_SYMBOLS; i++) {
        if(state->frequencies[i] > 0) {
            state->nodes[state->num_nodes].symbol = i;
            state->nodes[state->num_nodes].frequency = state->frequencies[i];
            state->nodes[state->num_nodes].left = 0xFFFF;
            state->nodes[state->num_nodes].right = 0xFFFF;
            state->nodes[state->num_nodes].parent = 0xFFFF;
            state->symbol_to_node[i] = state->num_nodes;
            state->num_nodes++;
        }
    }
    
    // Build tree by combining lowest frequency nodes
    uint16_t num_leaves = state->num_nodes;
    
    while(num_leaves > 1) {
        // Find two nodes with lowest frequency
        uint32_t min1_freq = 0xFFFFFFFF;
        uint32_t min2_freq = 0xFFFFFFFF;
        uint16_t min1_idx = 0xFFFF;
        uint16_t min2_idx = 0xFFFF;
        
        for(uint16_t i = 0; i < state->num_nodes; i++) {
            if(state->nodes[i].parent == 0xFFFF) {
                if(state->nodes[i].frequency < min1_freq) {
                    min2_freq = min1_freq;
                    min2_idx = min1_idx;
                    min1_freq = state->nodes[i].frequency;
                    min1_idx = i;
                } else if(state->nodes[i].frequency < min2_freq) {
                    min2_freq = state->nodes[i].frequency;
                    min2_idx = i;
                }
            }
        }
        
        if(min1_idx == 0xFFFF || min2_idx == 0xFFFF) break;
        
        // Create parent node
        uint16_t parent_idx = state->num_nodes++;
        state->nodes[parent_idx].frequency = state->nodes[min1_idx].frequency + 
                                            state->nodes[min2_idx].frequency;
        state->nodes[parent_idx].left = min1_idx;
        state->nodes[parent_idx].right = min2_idx;
        state->nodes[parent_idx].symbol = 0xFFFF;  // Internal node
        state->nodes[parent_idx].parent = 0xFFFF;
        
        state->nodes[min1_idx].parent = parent_idx;
        state->nodes[min2_idx].parent = parent_idx;
        
        num_leaves--;
    }
    
    // Find root
    state->root = 0xFFFF;
    for(uint16_t i = 0; i < state->num_nodes; i++) {
        if(state->nodes[i].parent == 0xFFFF) {
            state->root = i;
            break;
        }
    }
}

// Generate Huffman codes
void huffman_generate_codes(HuffmanState_t* state) {
    if(state->root == 0xFFFF) return;
    
    // Traverse tree to generate codes
    // Simplified: just assign codes based on depth
    for(uint16_t i = 0; i < state->num_nodes; i++) {
        if(state->nodes[i].symbol != 0xFFFF) {
            // Leaf node - calculate code by walking up tree
            uint32_t code = 0;
            uint8_t depth = 0;
            uint16_t node = i;
            
            while(state->nodes[node].parent != 0xFFFF) {
                uint16_t parent = state->nodes[node].parent;
                code = (code << 1) | ((state->nodes[parent].right == node) ? 1 : 0);
                depth++;
                node = parent;
            }
            
            // Reverse code
            uint32_t reversed = 0;
            for(uint8_t j = 0; j < depth; j++) {
                reversed = (reversed << 1) | ((code >> j) & 1);
            }
            
            state->nodes[i].code = reversed;
            state->nodes[i].code_length = depth;
        }
    }
}

// Huffman encode
uint32_t huffman_encode(const HuffmanState_t* state, const uint8_t* input,
                        uint32_t len, uint8_t* output) {
    uint32_t out_pos = 0;
    uint8_t bit_buffer = 0;
    uint8_t bit_count = 0;
    
    for(uint32_t i = 0; i < len; i++) {
        uint16_t node_idx = state->symbol_to_node[input[i]];
        uint32_t code = state->nodes[node_idx].code;
        uint8_t code_len = state->nodes[node_idx].code_length;
        
        // Add bits to buffer
        for(int8_t j = code_len - 1; j >= 0; j--) {
            bit_buffer = (bit_buffer << 1) | ((code >> j) & 1);
            bit_count++;
            
            if(bit_count == 8) {
                output[out_pos++] = bit_buffer;
                bit_buffer = 0;
                bit_count = 0;
                
                if(out_pos >= COMPRESSION_MAX_BLOCK_SIZE) break;
            }
        }
    }
    
    // Flush remaining bits
    if(bit_count > 0) {
        bit_buffer <<= (8 - bit_count);
        output[out_pos++] = bit_buffer;
    }
    
    return out_pos;
}

// Huffman decode
uint32_t huffman_decode(const HuffmanState_t* state, const uint8_t* input,
                        uint32_t len, uint8_t* output) {
    // Would implement tree traversal
    // For now, return 0
    UNUSED(state);
    UNUSED(input);
    UNUSED(len);
    UNUSED(output);
    return 0;
}

// LZ77 encode (simplified)
uint32_t lz77_encode(const uint8_t* input, uint32_t len, uint8_t* output,
                     uint16_t window_size, uint16_t lookahead_size) {
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    
    while(in_pos < len && out_pos < COMPRESSION_MAX_BLOCK_SIZE - 4) {
        // Find longest match in window
        uint16_t best_length = 0;
        uint16_t best_offset = 0;
        
        uint16_t window_start = (in_pos > window_size) ? (in_pos - window_size) : 0;
        uint16_t max_lookahead = (in_pos + lookahead_size < len) ? 
                                 lookahead_size : (len - in_pos);
        
        for(uint16_t i = window_start; i < in_pos; i++) {
            uint16_t match_len = 0;
            while(match_len < max_lookahead && 
                  input[i + match_len] == input[in_pos + match_len]) {
                match_len++;
            }
            
            if(match_len > best_length) {
                best_length = match_len;
                best_offset = in_pos - i;
            }
        }
        
        if(best_length >= 3) {
            // Encode as (offset, length) pair
            output[out_pos++] = 0x00;  // Match indicator
            output[out_pos++] = best_offset >> 8;
            output[out_pos++] = best_offset & 0xFF;
            output[out_pos++] = best_length;
            in_pos += best_length;
        } else {
            // Encode literal
            if(input[in_pos] == 0x00) {
                output[out_pos++] = 0x00;
                output[out_pos++] = 0xFF;  // Escape
            }
            output[out_pos++] = input[in_pos++];
        }
    }
    
    return out_pos;
}

// LZ77 decode
uint32_t lz77_decode(const uint8_t* input, uint32_t len, uint8_t* output) {
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    
    while(in_pos < len) {
        uint8_t byte = input[in_pos++];
        
        if(byte == 0x00) {
            if(in_pos >= len) break;
            uint8_t next = input[in_pos++];
            
            if(next == 0xFF) {
                // Escaped 0x00
                output[out_pos++] = 0x00;
            } else if(in_pos + 1 < len) {
                // Match
                uint16_t offset = (next << 8) | input[in_pos++];
                uint8_t length = input[in_pos++];
                
                for(uint8_t i = 0; i < length; i++) {
                    output[out_pos] = output[out_pos - offset];
                    out_pos++;
                }
            }
        } else {
            output[out_pos++] = byte;
        }
    }
    
    return out_pos;
}

// Compress pulse sequence
uint32_t compress_pulse_sequence(const Pulse_t* pulses, uint16_t count,
                                  uint8_t* output, uint32_t max_output) {
    if(count == 0 || !output || max_output == 0) return 0;
    
    uint32_t out_pos = 0;
    
    // Store count
    output[out_pos++] = count >> 8;
    output[out_pos++] = count & 0xFF;
    
    // Delta encode pulse widths
    uint16_t* widths = (uint16_t*)malloc(count * sizeof(uint16_t));
    if(!widths) return 0;
    
    for(uint16_t i = 0; i < count; i++) {
        widths[i] = pulses[i].width_us;
    }
    
    uint32_t compressed = delta_encode_16bit(widths, count, &output[out_pos]);
    out_pos += compressed;
    
    // Store levels (RLE encoded)
    uint8_t current_level = pulses[0].level;
    uint8_t run_length = 1;
    
    for(uint16_t i = 1; i < count && out_pos < max_output; i++) {
        if(pulses[i].level == current_level && run_length < 255) {
            run_length++;
        } else {
            output[out_pos++] = (run_length << 1) | current_level;
            current_level = pulses[i].level;
            run_length = 1;
        }
    }
    
    if(run_length > 0 && out_pos < max_output) {
        output[out_pos++] = (run_length << 1) | current_level;
    }
    
    free(widths);
    return out_pos;
}

// Decompress pulse sequence
uint32_t decompress_pulse_sequence(const uint8_t* input, uint32_t len,
                                    Pulse_t* pulses, uint16_t max_pulses) {
    if(len < 2) return 0;
    
    uint32_t in_pos = 0;
    uint16_t count = (input[in_pos] << 8) | input[in_pos + 1];
    in_pos += 2;
    
    if(count > max_pulses) count = max_pulses;
    
    // Decompress widths
    uint16_t* widths = (uint16_t*)malloc(count * sizeof(uint16_t));
    if(!widths) return 0;
    
    uint32_t decompressed = delta_decode_16bit(&input[in_pos], len - in_pos, widths);
    in_pos += decompressed;
    
    // Decompress levels
    uint16_t pulse_idx = 0;
    while(pulse_idx < count && in_pos < len) {
        uint8_t packed = input[in_pos++];
        uint8_t run_length = packed >> 1;
        uint8_t level = packed & 1;
        
        for(uint8_t i = 0; i < run_length && pulse_idx < count; i++) {
            pulses[pulse_idx].width_us = widths[pulse_idx];
            pulses[pulse_idx].level = level;
            pulses[pulse_idx].timestamp_us = 0;  // Would reconstruct
            pulse_idx++;
        }
    }
    
    free(widths);
    return pulse_idx;
}

// Find duplicate frames
uint32_t find_duplicate_frames(const Frame_t* frames, uint16_t count,
                                uint16_t* duplicates, uint16_t max_duplicates) {
    uint16_t dup_count = 0;
    
    for(uint16_t i = 0; i < count && dup_count < max_duplicates; i++) {
        for(uint16_t j = i + 1; j < count; j++) {
            if(frames[i].length == frames[j].length) {
                if(memcmp(frames[i].data, frames[j].data, frames[i].length) == 0) {
                    duplicates[dup_count++] = j;
                    break;
                }
            }
        }
    }
    
    return dup_count;
}

// Compress frame sequence
uint32_t compress_frame_sequence(const Frame_t* frames, uint16_t count,
                                  uint8_t* output, uint32_t max_output) {
    // Would implement frame deduplication and delta compression
    // For now, just serialize
    uint32_t out_pos = 0;
    
    for(uint16_t i = 0; i < count && out_pos + 64 < max_output; i++) {
        output[out_pos++] = frames[i].length;
        memcpy(&output[out_pos], frames[i].data, frames[i].length);
        out_pos += frames[i].length;
    }
    
    return out_pos;
}

// Select best compression algorithm
CompressionAlgorithm_t compression_select_algorithm(const uint8_t* sample_data,
                                                   uint32_t sample_len) {
    // Quick test of each algorithm on sample
    uint8_t test_output[256];
    uint32_t test_len;
    
    // Test delta
    test_len = delta_encode(sample_data, sample_len, test_output);
    float delta_ratio = (float)sample_len / (float)test_len;
    
    // Test RLE
    test_len = rle_encode(sample_data, sample_len, test_output);
    float rle_ratio = (float)sample_len / (float)test_len;
    
    // Pick best
    if(delta_ratio > rle_ratio && delta_ratio > 1.2f) {
        return COMPRESS_DELTA;
    } else if(rle_ratio > 1.2f) {
        return COMPRESS_RLE;
    }
    
    return COMPRESS_NONE;
}

// Estimate compression ratio
float compression_estimate_ratio(const uint8_t* data, uint32_t len,
                                  CompressionAlgorithm_t algorithm) {
    uint8_t test_output[256];
    uint32_t test_len = 0;
    
    switch(algorithm) {
        case COMPRESS_DELTA:
            test_len = delta_encode(data, len > 256 ? 256 : len, test_output);
            break;
        case COMPRESS_RLE:
            test_len = rle_encode(data, len > 256 ? 256 : len, test_output);
            break;
        default:
            return 1.0f;
    }
    
    if(test_len == 0) return 1.0f;
    return (float)(len > 256 ? 256 : len) / (float)test_len;
}

// Compress block with CRC
bool compression_compress_block(const uint8_t* input, uint32_t input_len,
                                 uint8_t* output, uint32_t* output_len,
                                 uint32_t* crc32) {
    // Would compress and calculate CRC
    // For now, just copy
    memcpy(output, input, input_len);
    *output_len = input_len;
    *crc32 = 0;  // Would calculate
    return true;
}

// Decompress block with CRC verification
bool compression_decompress_block(const uint8_t* input, uint32_t input_len,
                                   uint8_t* output, uint32_t* output_len,
                                   uint32_t expected_crc32) {
    // Would decompress and verify CRC
    UNUSED(expected_crc32);
    memcpy(output, input, input_len);
    *output_len = input_len;
    return true;
}

// Streaming compression (placeholder)
void compression_stream_init(CompressionAlgorithm_t algorithm) {
    UNUSED(algorithm);
}

bool compression_stream_process(const uint8_t* input, uint32_t len,
                                 uint8_t* output, uint32_t* output_len) {
    UNUSED(input);
    UNUSED(len);
    UNUSED(output);
    UNUSED(output_len);
    return true;
}

void compression_stream_finalize(uint8_t* output, uint32_t* output_len) {
    UNUSED(output);
    UNUSED(output_len);
}

// Save/load Huffman tree
void huffman_save_tree(const HuffmanState_t* state, uint8_t* buffer, uint32_t* len) {
    // Would serialize tree structure
    UNUSED(state);
    UNUSED(buffer);
    *len = 0;
}

bool huffman_load_tree(HuffmanState_t* state, const uint8_t* buffer, uint32_t len) {
    // Would deserialize tree structure
    UNUSED(buffer);
    UNUSED(len);
    memset(state, 0, sizeof(HuffmanState_t));
    return true;
}
