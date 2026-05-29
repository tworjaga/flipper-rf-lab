# API Reference

## Core Data Structures

### FlipperRFLabContext
Main application context structure.

```c
typedef struct {
    // Buffers
    uint8_t* dma_buffer;
    size_t dma_buffer_size;
    uint8_t* pulse_buffer;
    size_t pulse_buffer_size;
    uint8_t* frame_buffer;
    size_t frame_buffer_size;
    
    // Circular buffers
    CircularBuffer_t rx_buffer;
    CircularBuffer_t pulse_circ_buffer;
    
    // Configuration
    RFConfig_t rf_config;
    Session_t current_session;
    
    // State
    bool low_power_mode;
    bool capture_active;
    uint32_t frames_captured;
    
    // Telemetry
    SystemTelemetry_t telemetry;
    
    // Analysis results
    Fingerprint_t last_fingerprint;
    Cluster_t clusters[MAX_CLUSTERS];
} FlipperRFLabContext_t;
```

### RFConfig
RF transceiver configuration.

```c
typedef struct {
    uint32_t frequency_hz;
    ModulationType_t modulation;
    uint32_t data_rate_baud;
    int8_t tx_power_dbm;
    uint32_t bandwidth_hz;
    FrequencyBand_t band;
} RFConfig_t;
```

### Session
Capture session metadata.

```c
typedef struct {
    uint32_t id;
    uint32_t timestamp;
    uint32_t frequency_hz;
    uint32_t frame_count;
    uint32_t unique_devices;
    char filename[32];
    bool active;
} Session_t;
```

### Frame
Captured RF frame.

```c
typedef struct {
    uint32_t timestamp_us;
    uint16_t pulse_count;
    uint16_t duration_us;
    int16_t rssi_dbm;
    uint8_t data[MAX_FRAME_SIZE];
    uint16_t data_len;
    uint32_t fingerprint_hash;
} Frame_t;
```

### Pulse
Individual pulse timing.

```c
typedef struct {
    uint8_t level;      // 0 or 1
    uint16_t duration_us;
} Pulse_t;
```

### RFFingerprint
Device fingerprint signature.

```c
typedef struct {
    uint32_t drift_mean;
    uint32_t drift_variance;
    uint16_t rise_time_avg;
    uint16_t fall_time_avg;
    uint8_t clock_stability_ppm;
    uint8_t rssi_signature[16];
    uint16_t unique_hash;
} RFFingerprint_t;
```

## Hardware Abstraction Layer (HAL)

### CC1101 Driver

#### Initialization
```c
FuriStatus cc1101_driver_init(void);
void cc1101_driver_deinit(void);
void cc1101_reset(void);
```

#### Register Access
```c
uint8_t cc1101_read_register(uint8_t reg);
void cc1101_write_register(uint8_t reg, uint8_t value);
void cc1101_write_burst(uint8_t reg, const uint8_t* data, uint8_t len);
void cc1101_read_burst(uint8_t reg, uint8_t* data, uint8_t len);
void cc1101_send_command(uint8_t cmd);
```

#### Configuration
```c
void cc1101_set_frequency(uint32_t freq_hz);
void cc1101_set_data_rate(uint32_t baud);
void cc1101_set_bandwidth(uint32_t bw_hz);
void cc1101_set_tx_power(int8_t dbm);
void cc1101_set_modulation(ModulationType_t mod);
```

#### Operation Modes
```c
void cc1101_enter_rx_mode(void);
void cc1101_enter_tx_mode(void);
void cc1101_enter_idle_mode(void);
bool cc1101_has_data(void);
void cc1101_set_low_power_mode(bool enable);
```

#### Data Transfer
```c
uint8_t cc1101_receive_packet(uint8_t* data, uint8_t max_len);
void cc1101_transmit_packet(const uint8_t* data, uint8_t len);
int16_t cc1101_read_rssi(void);
uint8_t cc1101_get_status(void);
```

#### Presets
```c
void cc1101_load_preset_433mhz(void);
void cc1101_load_preset_868mhz(void);
void cc1101_load_preset_915mhz(void);
```

### Timer Precision

#### Initialization
```c
void timer_precision_init(void);
```

#### Timing Functions
```c
uint32_t timer_get_us(void);
uint32_t timer_get_ms(void);
void timer_delay_us(uint32_t us);
void timer_delay_ms(uint32_t ms);
```

#### DWT Cycle Counter
```c
void dwt_reset_cycle_counter(void);
uint32_t dwt_get_cycle_count(void);
uint32_t dwt_get_active_cycles(void);
uint32_t cycles_to_us(uint32_t cycles);
uint32_t us_to_cycles(uint32_t us);
```

### GPIO Manager

```c
FuriStatus gpio_manager_init(void);
void gpio_manager_deinit(void);

void gpio_set_pin_mode(uint16_t pin, GpioMode_t mode);
void gpio_write_pin(uint16_t pin, bool value);
bool gpio_read_pin(uint16_t pin);
void gpio_toggle_pin(uint16_t pin);

bool gpio_button_pressed(Button_t button);
bool gpio_button_held(Button_t button, uint32_t ms);
```

## Math Library

### Fixed-Point (Q15.16)

```c
// Type definition
typedef int32_t fixed_t;
#define FIXED_SCALE 65536

// Conversion
fixed_t float_to_fixed(float f);
float fixed_to_float(fixed_t x);
fixed_t int_to_fixed(int32_t i);

// Basic operations
fixed_t fixed_add(fixed_t a, fixed_t b);
fixed_t fixed_sub(fixed_t a, fixed_t b);
fixed_t fixed_mul(fixed_t a, fixed_t b);
fixed_t fixed_div(fixed_t a, fixed_t b);
fixed_t fixed_abs(fixed_t x);
fixed_t fixed_sqrt(fixed_t x);

// Trigonometry
fixed_t fixed_sin(fixed_t angle);
fixed_t fixed_cos(fixed_t angle);
fixed_t fixed_atan2(fixed_t y, fixed_t x);

// Advanced
fixed_t fixed_exp(fixed_t x);
fixed_t fixed_log(fixed_t x);
fixed_t fixed_pow(fixed_t base, fixed_t exp);
```

### Statistics

```c
// Welford's online algorithm
void welford_init(WelfordState_t* state);
void welford_add(WelfordState_t* state, int32_t value);
int32_t welford_mean(const WelfordState_t* state);
int32_t welford_variance(const WelfordState_t* state);
int32_t welford_std_dev(const WelfordState_t* state);

// Histogram
void histogram_init(Histogram_t* hist, uint8_t bins);
void histogram_add(Histogram_t* hist, uint8_t value);
uint8_t histogram_mode(const Histogram_t* hist);
uint8_t histogram_median(const Histogram_t* hist);

// Entropy
uint8_t shannon_entropy(const uint8_t* data, size_t len);
```

### Matrix Operations

```c
// 2x2 matrix
void mat2_mult(const fixed_t a[2][2], const fixed_t b[2][2], fixed_t result[2][2]);
fixed_t mat2_det(const fixed_t m[2][2]);
void mat2_inv(const fixed_t m[2][2], fixed_t result[2][2]);

// 3x3 matrix
void mat3_mult(const fixed_t a[3][3], const fixed_t b[3][3], fixed_t result[3][3]);
fixed_t mat3_det(const fixed_t m[3][3]);

// Vector operations
fixed_t vec2_dot(const fixed_t a[2], const fixed_t b[2]);
fixed_t vec2_norm(const fixed_t v[2]);
void vec2_normalize(fixed_t v[2]);

fixed_t vec3_dot(const fixed_t a[3], const fixed_t b[3]);
fixed_t vec3_norm(const fixed_t v[3]);
void vec3_normalize(fixed_t v[3]);
```

## Analysis Engines

### Fingerprinting

```c
FuriStatus fingerprinting_engine_init(void);
void fingerprinting_engine_deinit(void);

void fingerprint_extract(const Frame_t* frame, RFFingerprint_t* fingerprint);
uint8_t fingerprint_compare(const RFFingerprint_t* a, const RFFingerprint_t* b);
void fingerprint_save(const RFFingerprint_t* fingerprint, const char* name);
bool fingerprint_load(uint32_t hash, RFFingerprint_t* fingerprint);
```

### Clustering

```c
FuriStatus clustering_engine_init(void);
void clustering_engine_deinit(void);

void kmeans_cluster(const Pulse_t* pulses, uint16_t count, 
                    uint8_t k, Cluster_t* clusters);
void hierarchical_cluster(const Frame_t* frames, uint16_t count,
                          Cluster_t* clusters, uint8_t* linkage);
uint8_t silhouette_score(const Cluster_t* clusters, uint8_t k);
```

### Protocol Inference

```c
FuriStatus protocol_infer_init(void);
void protocol_infer_deinit(void);

ModulationType_t detect_modulation(const Pulse_t* pulses, uint16_t count);
EncodingType_t detect_encoding(const Pulse_t* pulses, uint16_t count);
uint32_t estimate_baud_rate(const Pulse_t* pulses, uint16_t count);
bool detect_preamble(const uint8_t* data, size_t len, 
                     uint8_t* preamble, size_t* preamble_len);
```

### Threat Model

```c
FuriStatus threat_model_init(void);
void threat_model_deinit(void);

RiskLevel_t assess_frame_risk(const Frame_t* frame);
uint8_t calculate_entropy(const uint8_t* data, size_t len);
bool detect_static_fields(const Frame_t* frames, uint16_t count,
                          uint8_t* static_mask, size_t len);
bool detect_rolling_code(const Frame_t* frames, uint16_t count);
VulnerabilityReport_t generate_report(const Session_t* session);
```

## Storage

### SD Manager

```c
FuriStatus sd_manager_init(void);
void sd_manager_deinit(void);

bool sd_is_present(void);
uint64_t sd_get_free_space(void);

FuriStatus session_create(Session_t* session);
FuriStatus session_write_frame(Session_t* session, const Frame_t* frame);
FuriStatus session_close(Session_t* session);
FuriStatus session_load(uint32_t session_id, Session_t* session);

FuriStatus export_csv(const Session_t* session, const char* filename);
FuriStatus export_json(const Session_t* session, const char* filename);
```

### Compression

```c
size_t delta_encode(const uint8_t* input, size_t len, uint8_t* output);
size_t delta_decode(const uint8_t* input, size_t len, uint8_t* output);

size_t rle_encode(const uint8_t* input, size_t len, uint8_t* output);
size_t rle_decode(const uint8_t* input, size_t len, uint8_t* output);

size_t huffman_encode(const uint8_t* input, size_t len, uint8_t* output);
size_t huffman_decode(const uint8_t* input, size_t len, uint8_t* output);

size_t lz77_compress(const uint8_t* input, size_t len, uint8_t* output);
size_t lz77_decompress(const uint8_t* input, size_t len, uint8_t* output);
```

## UI Components

### Main Menu

```c
void main_menu_init(ViewDispatcher* dispatcher);
void main_menu_deinit(void);

void main_menu_show(void);
void main_menu_hide(void);
void main_menu_set_selection(uint8_t index);
```

### Capture View

```c
void capture_view_init(void);
void capture_view_deinit(void);

void capture_view_show(void);
void capture_view_update_rssi(int16_t rssi);
void capture_view_update_info(const char* info);
void capture_view_show_frame(const Frame_t* frame);
```

### Analysis View

```c
void analysis_view_init(void);
void analysis_view_deinit(void);

void analysis_view_show_histogram(const Histogram_t* hist);
void analysis_view_show_waveform(const Pulse_t* pulses, uint16_t count);
void analysis_view_show_fingerprint(const RFFingerprint_t* fingerprint);
```

## Research Tools

### Telemetry

```c
void telemetry_init(void);
void telemetry_deinit(void);

void telemetry_record_event(TelemetryEvent_t event);
void telemetry_get_stats(TelemetryStats_t* stats);
void telemetry_export(const char* filename);
```

### Raw Dump

```c
void raw_dump_init(void);
void raw_dump_deinit(void);

void raw_dump_start(const char* filename);
void raw_dump_stop(void);
void raw_dump_pulse(const Pulse_t* pulse);
void raw_dump_frame(const Frame_t* frame);
```

## Error Codes

```c
typedef enum {
    FURI_STATUS_OK = 0,
    FURI_STATUS_ERROR = -1,
    FURI_STATUS_ERROR_INVALID_PARAMETER = -2,
    FURI_STATUS_ERROR_OUT_OF_MEMORY = -3,
    FURI_STATUS_ERROR_TIMEOUT = -4,
    FURI_STATUS_ERROR_BUSY = -5,
    FURI_STATUS_ERROR_NOT_FOUND = -6,
    FURI_STATUS_ERROR_ALREADY_EXISTS = -7,
    FURI_STATUS_ERROR_ACCESS_DENIED = -8,
    FURI_STATUS_ERROR_NOT_IMPLEMENTED = -9,
    FURI_STATUS_ERROR_INTERNAL = -10,
} FuriStatus;
```

## Constants

```c
// Buffer sizes
#define SPI_DMA_BUFFER_SIZE 4096
#define PULSE_BUFFER_SIZE 8192
#define FRAME_BUFFER_SIZE 16384
#define MAX_FRAME_SIZE 64
#define MAX_PULSE_COUNT 1000
#define MAX_CLUSTERS 5

// Timing
#define SYSTEM_CORE_CLOCK 64000000
#define MAX_STACK_DEPTH 4096
#define ISR_LATENCY_MAX_US 10

// RF
#define MIN_FREQUENCY_HZ 300000000
#define MAX_FREQUENCY_HZ 928000000
#define DEFAULT_DATA_RATE 2400
#define DEFAULT_TX_POWER 0

// UI
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define UI_UPDATE_HZ 30
```

---

For more details, see the [source code](../core/) or [User Guide](USER_GUIDE.md).
