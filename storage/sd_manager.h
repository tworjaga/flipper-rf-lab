#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <furi.h>
#include <storage/storage.h>
#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SD CARD FILE SYSTEM MANAGEMENT
// ============================================================================

#define SD_BASE_PATH        "/ext/apps_data/flipper_rf"
#define CAPTURES_PATH       SD_BASE_PATH "/captures"
#define FINGERPRINTS_PATH   SD_BASE_PATH "/fingerprints"
#define LOGS_PATH           SD_BASE_PATH "/logs"
#define EXPORTS_PATH        SD_BASE_PATH "/exports"
#define CONFIG_PATH         SD_BASE_PATH "/config"

#define MAX_FILENAME_LEN    64
#define MAX_PATH_LEN        256
#define MAX_SESSIONS        999
#define MAX_FILE_HANDLES    4

// ============================================================================
// FILE TYPES
// ============================================================================

typedef enum {
    FILE_TYPE_RAW,
    FILE_TYPE_ANALYZED,
    FILE_TYPE_METADATA,
    FILE_TYPE_FINGERPRINT,
    FILE_TYPE_LOG,
    FILE_TYPE_EXPORT,
    FILE_TYPE_CONFIG
} FileType_t;

typedef enum {
    EXPORT_FORMAT_CSV,
    EXPORT_FORMAT_JSON,
    EXPORT_FORMAT_BINARY,
    EXPORT_FORMAT_TEXT
} ExportFormat_t;

// ============================================================================
// FILE HANDLE STRUCTURE
// ============================================================================

typedef struct {
    Storage* storage;
    File* file;
    char path[MAX_PATH_LEN];
    FileType_t type;
    bool is_open;
    uint32_t bytes_written;
    uint32_t bytes_read;
    uint32_t open_time;
} FileHandle_t;

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

typedef struct {
    uint16_t session_id;
    char session_name[SESSION_NAME_LEN];
    char timestamp[20];  // YYYY-MM-DD_HH-MM-SS
    uint32_t num_frames;
    uint32_t duration_ms;
    uint32_t file_size;
    bool has_raw;
    bool has_analyzed;
    bool has_metadata;
} SessionInfo_t;

typedef struct {
    SessionInfo_t sessions[MAX_SESSIONS];
    uint16_t count;
    uint16_t current_session;
} SessionIndex_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus sd_manager_init(void);
void sd_manager_deinit(void);
bool sd_manager_is_card_present(void);

// Directory management
bool sd_manager_ensure_directories(void);
bool sd_manager_create_session_directory(uint16_t session_id, char* path_out, size_t path_size);

// File operations
FileHandle_t* sd_manager_open_file(const char* path, FileType_t type, bool write);
void sd_manager_close_file(FileHandle_t* handle);
bool sd_manager_write(FileHandle_t* handle, const uint8_t* data, uint32_t len);
bool sd_manager_read(FileHandle_t* handle, uint8_t* data, uint32_t len);
bool sd_manager_write_string(FileHandle_t* handle, const char* str);

// Session management
uint16_t sd_manager_create_session(const char* name);
bool sd_manager_close_session(uint16_t session_id);
bool sd_manager_load_session_index(void);
bool sd_manager_save_session_index(void);
SessionInfo_t* sd_manager_get_session(uint16_t session_id);
bool sd_manager_delete_session(uint16_t session_id);

// Data export
bool sd_manager_export_session(uint16_t session_id, ExportFormat_t format, const char* filename);
bool sd_manager_export_fingerprint(const RFFingerprint_t* fingerprint, const char* device_name);
bool sd_manager_export_telemetry(const SystemTelemetry_t* telemetry, const char* filename);

// Compression support
bool sd_manager_write_compressed(FileHandle_t* handle, const uint8_t* data, uint32_t len);
bool sd_manager_read_compressed(FileHandle_t* handle, uint8_t* data, uint32_t max_len, uint32_t* out_len);

// Configuration
bool sd_manager_load_config(RFConfig_t* config);
bool sd_manager_save_config(const RFConfig_t* config);

// Logging
bool sd_manager_log_event(const char* event, const char* details);
bool sd_manager_log_system_status(const SystemTelemetry_t* telemetry);

// Utility functions
uint64_t sd_manager_get_free_space(void);
uint64_t sd_manager_get_total_space(void);
bool sd_manager_check_space(uint64_t required_bytes);
void sd_manager_format_path(char* out, size_t out_size, const char* base, const char* filename);

// Rolling log buffer
bool sd_manager_init_rolling_log(uint32_t max_size_mb);
bool sd_manager_write_rolling_log(const uint8_t* data, uint32_t len);
void sd_manager_flush_rolling_log(void);

// File listing
typedef void (*FileEnumCallback)(const char* path, uint32_t size, void* context);
bool sd_manager_enum_files(const char* directory, FileEnumCallback callback, void* context);

#ifdef __cplusplus
}
#endif

#endif // SD_MANAGER_H
