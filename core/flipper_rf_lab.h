#ifndef FLIPPER_RF_LAB_H
#define FLIPPER_RF_LAB_H

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// HARDWARE SPECIFICATIONS - STM32WB55RG CONSTRAINTS
// ============================================================================

#define SYSTEM_CORE_CLOCK       64000000UL      // 64 MHz
#define DWT_CYCCNT_US           64              // Cycles per microsecond
#define MAX_STACK_DEPTH         4096            // 4KB per task
#define TOTAL_RAM_AVAILABLE     (180 * 1024)    // ~180KB for application
#define CC1101_FIFO_SIZE        64              // TX/RX FIFO size
#define SPI_DMA_BUFFER_SIZE     256             // DMA burst transfer size

// ============================================================================
// BUFFER SIZES - STATIC ALLOCATION ONLY
// ============================================================================

#define PULSE_BUFFER_SIZE       8192            // 8KB for pulse timing data
#define FRAME_BUFFER_SIZE       16384           // 16KB for frame storage
#define MAX_PULSE_COUNT         4096            // Maximum pulses per capture
#define MAX_FRAME_COUNT         256             // Maximum frames in session
#define MAX_CLUSTERS            5               // K-means clustering limit
#define FINGERPRINT_VECTOR_SIZE 32              // Bytes per fingerprint
#define MAX_DEVICE_DB_ENTRIES   128             // Device fingerprint database
#define HISTOGRAM_BINS          256             // Timing histogram resolution
#define SESSION_NAME_LEN        32              // Session identifier length

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

#define TIMING_PRECISION_US     1               // 1 microsecond resolution
#define MAX_PULSE_WIDTH_US      65535           // 16-bit pulse width limit
#define MIN_PULSE_WIDTH_US      10              // Minimum detectable pulse
#define FRAME_TIMEOUT_US        10000           // 10ms inter-frame gap
#define SPECTRUM_DWELL_MS       10              // 10ms per frequency step
#define SPECTRUM_RANGE_MHZ      628             // 300-928 MHz range

// ============================================================================
// SYSTEM STATES
// ============================================================================

typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_CAPTURING,
    STATE_ANALYZING,
    STATE_SPECTRUM_SCAN,
    STATE_PASSIVE_MONITOR,
    STATE_REPLAY_MODE,
    STATE_RESEARCH_MODE,
    STATE_SHUTDOWN
} SystemState_t;

// ============================================================================
// RF CONFIGURATION
// ============================================================================

typedef enum {
    MOD_2FSK = 0,
    MOD_4FSK,
    MOD_GFSK,
    MOD_MSK,
    MOD_OOK,
    MOD_ASK,
    MOD_COUNT
} ModulationType_t;

typedef enum {
    BAND_315MHZ = 0,
    BAND_433MHZ,
    BAND_868MHZ,
    BAND_915MHZ,
    BAND_CUSTOM
} FrequencyBand_t;

typedef struct {
    uint32_t frequency_hz;          // Center frequency
    uint32_t data_rate_baud;         // Baud rate
    uint32_t channel_bw_hz;          // Channel filter bandwidth
    uint8_t tx_power_dbm;            // TX power (-20 to +10)
    ModulationType_t modulation;    // Modulation type
    FrequencyBand_t band;            // Frequency band
    bool manchester_encoding;        // Manchester encoding enabled
    bool whitening;                  // Data whitening enabled
    uint8_t sync_word[2];            // Sync word bytes
} RFConfig_t;

// ============================================================================
// PULSE TIMING DATA
// ============================================================================

typedef struct {
    uint16_t width_us;              // Pulse width in microseconds
    uint8_t level;                  // Signal level (0=low, 1=high)
    uint32_t timestamp_us;          // Absolute timestamp (1us resolution)
} Pulse_t;

typedef struct {
    Pulse_t pulses[MAX_PULSE_COUNT];
    uint16_t count;
    uint16_t head;
    uint16_t tail;
    bool overflow;
} PulseBuffer_t;

// ============================================================================
// FRAME DATA
// ============================================================================

typedef struct {
    uint8_t data[64];               // Demodulated frame data
    uint8_t length;                 // Frame length in bytes
    uint32_t timestamp_us;          // Capture timestamp
    uint16_t rssi_dbm;              // RSSI at capture time
    uint32_t frequency_hz;          // Frequency of capture
    uint16_t pulse_start_idx;       // Index into pulse buffer
    uint16_t pulse_count;           // Number of pulses in frame
    uint32_t duration_us;           // Total frame duration
    uint16_t crc;                   // Calculated CRC
    bool crc_valid;                 // CRC validation result
} Frame_t;

typedef struct {
    Frame_t frames[MAX_FRAME_COUNT];
    uint16_t count;
    uint16_t current_idx;
    uint32_t session_start_us;
    char session_id[SESSION_NAME_LEN];
    RFConfig_t config;
} Session_t;

// ============================================================================
// RF FINGERPRINTING
// ============================================================================

typedef struct {
    uint32_t drift_mean;            // Mean inter-frame drift (us)
    uint32_t drift_variance;        // Variance of timing (us^2)
    uint16_t rise_time_avg;         // Average rise slope (RSSI units/us)
    uint16_t fall_time_avg;         // Average fall slope
    uint8_t clock_stability_ppm;    // Estimated clock ppm deviation
    uint8_t rssi_signature[16];     // Envelope characteristic points
    uint16_t unique_hash;           // CRC16 of above for quick compare
} RFFingerprint_t;

typedef struct {
    RFFingerprint_t fingerprints[MAX_DEVICE_DB_ENTRIES];
    uint8_t count;
    char device_names[MAX_DEVICE_DB_ENTRIES][16];
    uint32_t last_seen[MAX_DEVICE_DB_ENTRIES];
    uint16_t match_count[MAX_DEVICE_DB_ENTRIES];
} DeviceDatabase_t;

// ============================================================================
// TIMING HISTOGRAM
// ============================================================================

typedef struct {
    uint16_t bins[HISTOGRAM_BINS];  // Bin counts
    uint16_t min_val;               // Minimum value in range
    uint16_t max_val;               // Maximum value in range
    uint32_t total_samples;         // Total samples added
    uint16_t peak_bin;              // Bin with maximum count
    uint16_t peak_count;            // Count at peak bin
} TimingHistogram_t;

// ============================================================================
// CLUSTERING
// ============================================================================

typedef struct {
    int16_t x;                      // X coordinate (fixed-point)
    int16_t y;                      // Y coordinate (fixed-point)
    uint16_t count;                 // Points in cluster
    uint16_t id;                    // Cluster identifier
} ClusterCenter_t;

typedef struct {
    ClusterCenter_t centers[MAX_CLUSTERS];
    uint8_t num_clusters;
    uint8_t assigned_cluster[MAX_PULSE_COUNT];  // Cluster assignment per pulse
    uint32_t iterations;            // K-means iterations performed
    bool converged;                 // Convergence flag
} ClusteringResult_t;

// ============================================================================
// THREAT MODELING
// ============================================================================

typedef enum {
    RISK_LOW = 0,                   // Rolling code, encryption, proper auth
    RISK_MEDIUM = 1,                // Checksum present but no rolling code
    RISK_HIGH = 2,                  // Static frames with checksum only
    RISK_CRITICAL = 3               // No protection, raw commands, replayable
} RiskLevel_t;

typedef struct {
    RiskLevel_t level;
    uint8_t entropy_bits;           // Shannon entropy of payload
    bool has_checksum;              // Checksum/CRC detected
    bool has_rolling_code;          // Rolling code pattern detected
    bool is_static;                 // Static frame content
    uint16_t static_ratio;          // Percentage of static bits (0-100)
    uint16_t vulnerability_score;   // 0-1000 composite score
    char description[64];           // Human-readable assessment
} ThreatAssessment_t;

// ============================================================================
// TELEMETRY
// ============================================================================

typedef struct {
    uint32_t cpu_load_percent;        // Current CPU load
    uint32_t frames_per_second;       // Processing rate
    uint32_t buffer_utilization;      // RX buffer fill percentage
    uint32_t isr_latency_max_us;      // Maximum ISR latency observed
    uint32_t sd_write_latency_ms;     // SD card write latency
    uint32_t cc1101_irq_count;        // CC1101 interrupt count
    uint32_t dma_transfer_count;      // DMA transfer count
    uint32_t uptime_seconds;          // System uptime
    float battery_voltage;            // Current battery voltage
    uint8_t temperature_c;            // System temperature (if available)
} SystemTelemetry_t;

// ============================================================================
// CIRCULAR BUFFER
// ============================================================================

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    FuriMutex* mutex;
} CircularBuffer_t;

// ============================================================================
// PLATFORM CONTEXT
// ============================================================================

typedef struct {
    // Buffer pointers (statically allocated)
    uint8_t* dma_buffer;
    uint32_t dma_buffer_size;
    uint8_t* pulse_buffer;
    uint32_t pulse_buffer_size;
    uint8_t* frame_buffer;
    uint32_t frame_buffer_size;
    
    // Circular buffers
    CircularBuffer_t rx_buffer;
    CircularBuffer_t pulse_circ_buffer;
    
    // Current session
    Session_t current_session;
    
    // Device database
    DeviceDatabase_t device_db;
    
    // System telemetry
    SystemTelemetry_t telemetry;
    
    // Configuration
    RFConfig_t rf_config;
    bool deterministic_mode;
    bool low_power_mode;
    
    // Statistics
    uint32_t total_captures;
    uint32_t total_frames_processed;
    uint32_t total_devices_identified;
    
} FlipperRFLabContext;

// ============================================================================
// FUNCTION DECLARATIONS - CORE
// ============================================================================

// Main entry point
int32_t flipper_rf_lab_main(void* p);

// Memory management
bool memory_pools_init(void);
void* memory_pool_alloc(uint32_t size);
void memory_pool_free(void* ptr);

// Circular buffer operations
void circular_buffer_init(CircularBuffer_t* cb, uint8_t* buffer, uint32_t size);
bool circular_buffer_write(CircularBuffer_t* cb, uint8_t data);
bool circular_buffer_read(CircularBuffer_t* cb, uint8_t* data);
uint32_t circular_buffer_count(CircularBuffer_t* cb);
void circular_buffer_clear(CircularBuffer_t* cb);

// Worker threads
static int32_t rf_capture_worker(void* context);
static int32_t ui_update_worker(void* context);
static int32_t analysis_worker(void* context);

// System functions
static void update_system_telemetry(void);
static void enter_low_power_mode(void);

// Capture functions
void capture_frame_burst(void);
void spectrum_sweep_step(void);
void passive_monitor_cycle(void);

// Analysis functions
bool has_pending_analysis(void);
void process_next_analysis_task(void);

// Display functions
void update_display(void);

// ============================================================================
// EXTERN DECLARATIONS
// ============================================================================

// These will be defined in other modules
extern FuriStatus cc1101_driver_init(void);
extern void cc1101_driver_deinit(void);
extern FuriStatus gpio_manager_init(void);
extern FuriStatus sd_manager_init(void);
extern void sd_manager_deinit(void);
extern void fixed_point_init(void);
extern FuriStatus fingerprinting_engine_init(void);
extern FuriStatus clustering_engine_init(void);
extern FuriStatus threat_model_init(void);
extern void main_menu_init(ViewDispatcher* view_dispatcher);

// DWT cycle counter functions
extern void timer_precision_init(void);
extern uint32_t dwt_get_active_cycles(void);
extern void dwt_reset_cycle_counter(void);

#ifdef __cplusplus
}
#endif

#endif // FLIPPER_RF_LAB_H
