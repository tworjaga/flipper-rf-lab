#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SIGNAL COMPRESSION ENGINE
// Delta encoding, RLE, and Huffman coding
// ============================================================================

#define COMPRESSION_MAX_BLOCK_SIZE  1024
#define COMPRESSION_MAX_SYMBOLS     256
#define HUFFMAN_MAX_CODES           512
#define RLE_MAX_RUN_LENGTH          255

// Compression algorithms
typedef enum {
    COMPRESS_NONE = 0,
    COMPRESS_DELTA,
    COMPRESS_RLE,
    COMPRESS_HUFFMAN,
    COMPRESS_LZ77,
    COMPRESS_ADAPTIVE    // Auto-select best algorithm
} CompressionAlgorithm_t;

// Compression statistics
typedef struct {
    uint32_t original_size;
    uint32_t compressed_size;
    float ratio;
    CompressionAlgorithm_t algorithm;
    uint32_t encode_time_us;
    uint32_t decode_time_us;
} CompressionStats_t;

// Huffman tree node
typedef struct {
    uint16_t symbol;
    uint32_t frequency;
    uint16_t left;
    uint16_t right;
    uint16_t parent;
    uint8_t depth;
    uint32_t code;
    uint8_t code_length;
} HuffmanNode_t;

// Huffman coding state
typedef struct {
    HuffmanNode_t nodes[HUFFMAN_MAX_CODES];
    uint16_t num_nodes;
    uint16_t root;
    uint16_t symbol_to_node[COMPRESSION_MAX_SYMBOLS];
    uint32_t frequencies[COMPRESSION_MAX_SYMBOLS];
    bool initialized;
} HuffmanState_t;

// RLE state
typedef struct {
    uint8_t last_symbol;
    uint8_t run_length;
    bool in_run;
} RLEState_t;

// Delta encoding state
typedef struct {
    uint8_t last_value;
    uint8_t prediction;
} DeltaState_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
void compression_init(void);
void compression_deinit(void);

// High-level compression
bool compress_data(const uint8_t* input, uint32_t input_len,
                   uint8_t* output, uint32_t* output_len,
                   CompressionAlgorithm_t algorithm,
                   CompressionStats_t* stats);

bool decompress_data(const uint8_t* input, uint32_t input_len,
                     uint8_t* output, uint32_t* output_len,
                     CompressionStats_t* stats);

// Delta encoding
uint32_t delta_encode(const uint8_t* input, uint32_t len, uint8_t* output);
uint32_t delta_decode(const uint8_t* input, uint32_t len, uint8_t* output);
uint32_t delta_encode_16bit(const uint16_t* input, uint32_t len, uint8_t* output);
uint32_t delta_decode_16bit(const uint8_t* input, uint32_t len, uint16_t* output);

// Run-length encoding
uint32_t rle_encode(const uint8_t* input, uint32_t len, uint8_t* output);
uint32_t rle_decode(const uint8_t* input, uint32_t len, uint8_t* output);
uint32_t rle_encode_adaptive(const uint8_t* input, uint32_t len, uint8_t* output);

// Huffman coding
void huffman_init(HuffmanState_t* state);
void huffman_build_tree(HuffmanState_t* state, const uint8_t* data, uint32_t len);
void huffman_generate_codes(HuffmanState_t* state);
uint32_t huffman_encode(const HuffmanState_t* state, const uint8_t* input, 
                        uint32_t len, uint8_t* output);
uint32_t huffman_decode(const HuffmanState_t* state, const uint8_t* input,
                        uint32_t len, uint8_t* output);
void huffman_save_tree(const HuffmanState_t* state, uint8_t* buffer, uint32_t* len);
bool huffman_load_tree(HuffmanState_t* state, const uint8_t* buffer, uint32_t len);

// LZ77 sliding window compression
uint32_t lz77_encode(const uint8_t* input, uint32_t len, uint8_t* output,
                     uint16_t window_size, uint16_t lookahead_size);
uint32_t lz77_decode(const uint8_t* input, uint32_t len, uint8_t* output);

// Pulse sequence compression (specialized for RF data)
uint32_t compress_pulse_sequence(const Pulse_t* pulses, uint16_t count, 
                                  uint8_t* output, uint32_t max_output);
uint32_t decompress_pulse_sequence(const uint8_t* input, uint32_t len,
                                    Pulse_t* pulses, uint16_t max_pulses);

// Frame deduplication
uint32_t find_duplicate_frames(const Frame_t* frames, uint16_t count,
                                uint16_t* duplicates, uint16_t max_duplicates);
uint32_t compress_frame_sequence(const Frame_t* frames, uint16_t count,
                                  uint8_t* output, uint32_t max_output);

// Streaming compression
void compression_stream_init(CompressionAlgorithm_t algorithm);
bool compression_stream_process(const uint8_t* input, uint32_t len,
                                 uint8_t* output, uint32_t* output_len);
void compression_stream_finalize(uint8_t* output, uint32_t* output_len);

// Compression analysis
CompressionAlgorithm_t compression_select_algorithm(const uint8_t* sample_data, 
                                                   uint32_t sample_len);
float compression_estimate_ratio(const uint8_t* data, uint32_t len,
                                  CompressionAlgorithm_t algorithm);

// Block-based compression for SD storage
bool compression_compress_block(const uint8_t* input, uint32_t input_len,
                                 uint8_t* output, uint32_t* output_len,
                                 uint32_t* crc32);
bool compression_decompress_block(const uint8_t* input, uint32_t input_len,
                                   uint8_t* output, uint32_t* output_len,
                                   uint32_t expected_crc32);

#ifdef __cplusplus
}
#endif

#endif // COMPRESSION_H
