#include "sd_manager.h"
#include <datetime/datetime.h>

#define TAG "SD_MGR"

// Static state
static Storage* storage = NULL;
static SessionIndex_t session_index;
static bool sd_initialized = false;
static uint32_t rolling_log_size = 0;
static uint32_t rolling_log_max_size = 0;
static File* rolling_log_file = NULL;

// Initialize SD manager
FuriStatus sd_manager_init(void) {
    if(sd_initialized) {
        return FuriStatusOk;
    }
    
    FURI_LOG_I(TAG, "Initializing SD manager");
    
    // Open storage record
    storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return FuriStatusError;
    }
    
    // Check if SD card is present
    if(!sd_manager_is_card_present()) {
        FURI_LOG_W(TAG, "SD card not present");
        furi_record_close(RECORD_STORAGE);
        return FuriStatusError;
    }
    
    // Ensure directory structure exists
    if(!sd_manager_ensure_directories()) {
        FURI_LOG_E(TAG, "Failed to create directory structure");
        furi_record_close(RECORD_STORAGE);
        return FuriStatusError;
    }
    
    // Load session index
    sd_manager_load_session_index();
    
    sd_initialized = true;
    FURI_LOG_I(TAG, "SD manager initialized, %d sessions found", session_index.count);
    
    return FuriStatusOk;
}

void sd_manager_deinit(void) {
    if(!sd_initialized) return;
    
    // Close rolling log if open
    if(rolling_log_file) {
        storage_file_close(rolling_log_file);
        storage_file_free(rolling_log_file);
        rolling_log_file = NULL;
    }
    
    // Save session index
    sd_manager_save_session_index();
    
    // Close storage
    furi_record_close(RECORD_STORAGE);
    storage = NULL;
    
    sd_initialized = false;
    FURI_LOG_I(TAG, "SD manager deinitialized");
}

// Check if SD card is present
bool sd_manager_is_card_present(void) {
    if(!storage) return false;
    return storage_sd_status(storage) == FSDStatusOK;
}

// Ensure all required directories exist
bool sd_manager_ensure_directories(void) {
    if(!storage) return false;
    
    const char* dirs[] = {
        SD_BASE_PATH,
        CAPTURES_PATH,
        FINGERPRINTS_PATH,
        LOGS_PATH,
        EXPORTS_PATH,
        CONFIG_PATH
    };
    
    for(uint8_t i = 0; i < 6; i++) {
        if(!storage_simply_mkdir(storage, dirs[i])) {
            FURI_LOG_W(TAG, "Failed to create directory: %s", dirs[i]);
            // Continue anyway - directory might already exist
        }
    }
    
    return true;
}

// Create session directory
bool sd_manager_create_session_directory(uint16_t session_id, char* path_out, size_t path_size) {
    snprintf(path_out, path_size, "%s/session_%03d", CAPTURES_PATH, session_id);
    
    if(!storage_simply_mkdir(storage, path_out)) {
        FURI_LOG_E(TAG, "Failed to create session directory: %s", path_out);
        return false;
    }
    
    // Create subdirectories
    char subdir[MAX_PATH_LEN];
    snprintf(subdir, sizeof(subdir), "%s/raw", path_out);
    storage_simply_mkdir(storage, subdir);
    
    snprintf(subdir, sizeof(subdir), "%s/analyzed", path_out);
    storage_simply_mkdir(storage, subdir);
    
    return true;
}

// Open file
FileHandle_t* sd_manager_open_file(const char* path, FileType_t type, bool write) {
    if(!storage) return NULL;
    
    FileHandle_t* handle = malloc(sizeof(FileHandle_t));
    if(!handle) return NULL;
    
    handle->storage = storage;
    handle->file = storage_file_alloc(storage);
    handle->type = type;
    handle->is_open = false;
    handle->bytes_written = 0;
    handle->bytes_read = 0;
    handle->open_time = furi_get_tick();
    
    strncpy(handle->path, path, MAX_PATH_LEN - 1);
    handle->path[MAX_PATH_LEN - 1] = '\0';
    
    FS_AccessMode access = write ? FS_ACCESS_MODE_WRITE : FS_ACCESS_MODE_READ;
    FS_OpenMode open_mode = write ? (FS_OPEN_MODE_WRITE | FS_OPEN_MODE_CREATE_ALWAYS) : FS_OPEN_MODE_READ;
    
    if(!storage_file_open(handle->file, path, access, open_mode)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
        storage_file_free(handle->file);
        free(handle);
        return NULL;
    }
    
    handle->is_open = true;
    return handle;
}

// Close file
void sd_manager_close_file(FileHandle_t* handle) {
    if(!handle) return;
    
    if(handle->is_open && handle->file) {
        storage_file_close(handle->file);
        storage_file_free(handle->file);
    }
    
    uint32_t duration = furi_get_tick() - handle->open_time;
    FURI_LOG_D(TAG, "File closed: %s (duration: %lu ms)", handle->path, duration);
    
    free(handle);
}

// Write data to file
bool sd_manager_write(FileHandle_t* handle, const uint8_t* data, uint32_t len) {
    if(!handle || !handle->is_open) return false;
    
    uint32_t written = storage_file_write(handle->file, data, len);
    if(written != len) {
        FURI_LOG_E(TAG, "Write failed: wrote %lu of %lu bytes", written, len);
        return false;
    }
    
    handle->bytes_written += written;
    return true;
}

// Read data from file
bool sd_manager_read(FileHandle_t* handle, uint8_t* data, uint32_t len) {
    if(!handle || !handle->is_open) return false;
    
    uint32_t read = storage_file_read(handle->file, data, len);
    handle->bytes_read += read;
    
    return (read == len);
}

// Write string to file
bool sd_manager_write_string(FileHandle_t* handle, const char* str) {
    return sd_manager_write(handle, (const uint8_t*)str, strlen(str));
}

// Create new session
uint16_t sd_manager_create_session(const char* name) {
    if(session_index.count >= MAX_SESSIONS) {
        FURI_LOG_E(TAG, "Maximum sessions reached");
        return 0;
    }
    
    uint16_t session_id = session_index.count + 1;
    SessionInfo_t* info = &session_index.sessions[session_index.count];
    
    info->session_id = session_id;
    strncpy(info->session_name, name, SESSION_NAME_LEN - 1);
    info->session_name[SESSION_NAME_LEN - 1] = '\0';
    
    // Get current timestamp
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    snprintf(info->timestamp, sizeof(info->timestamp), 
             "%04d-%02d-%02d_%02d-%02d-%02d",
             datetime.year, datetime.month, datetime.day,
             datetime.hour, datetime.minute, datetime.second);
    
    info->num_frames = 0;
    info->duration_ms = 0;
    info->file_size = 0;
    info->has_raw = false;
    info->has_analyzed = false;
    info->has_metadata = false;
    
    // Create session directory
    char path[MAX_PATH_LEN];
    if(!sd_manager_create_session_directory(session_id, path, sizeof(path))) {
        return 0;
    }
    
    session_index.count++;
    session_index.current_session = session_id;
    
    FURI_LOG_I(TAG, "Created session %d: %s", session_id, name);
    
    return session_id;
}

// Close session
bool sd_manager_close_session(uint16_t session_id) {
    SessionInfo_t* info = sd_manager_get_session(session_id);
    if(!info) return false;
    
    // Update session info
    info->duration_ms = furi_get_tick();  // Would calculate actual duration
    
    // Save metadata
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_%03d/metadata.json", 
             CAPTURES_PATH, session_id);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_METADATA, true);
    if(file) {
        char metadata[512];
        snprintf(metadata, sizeof(metadata),
            "{\n"
            "  \"session_id\": %d,\n"
            "  \"name\": \"%s\",\n"
            "  \"timestamp\": \"%s\",\n"
            "  \"frames\": %lu,\n"
            "  \"duration_ms\": %lu,\n"
            "  \"has_raw\": %s,\n"
            "  \"has_analyzed\": %s\n"
            "}\n",
            info->session_id,
            info->session_name,
            info->timestamp,
            info->num_frames,
            info->duration_ms,
            info->has_raw ? "true" : "false",
            info->has_analyzed ? "true" : "false"
        );
        
        sd_manager_write_string(file, metadata);
        sd_manager_close_file(file);
        info->has_metadata = true;
    }
    
    FURI_LOG_I(TAG, "Closed session %d", session_id);
    return true;
}

// Load session index
bool sd_manager_load_session_index(void) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_index.bin", SD_BASE_PATH);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, false);
    if(!file) {
        // No existing index, start fresh
        memset(&session_index, 0, sizeof(session_index));
        return true;
    }
    
    bool success = sd_manager_read(file, (uint8_t*)&session_index, sizeof(session_index));
    sd_manager_close_file(file);
    
    if(!success) {
        memset(&session_index, 0, sizeof(session_index));
        return false;
    }
    
    return true;
}

// Save session index
bool sd_manager_save_session_index(void) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_index.bin", SD_BASE_PATH);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, true);
    if(!file) return false;
    
    bool success = sd_manager_write(file, (const uint8_t*)&session_index, sizeof(session_index));
    sd_manager_close_file(file);
    
    return success;
}

// Get session info
SessionInfo_t* sd_manager_get_session(uint16_t session_id) {
    for(uint16_t i = 0; i < session_index.count; i++) {
        if(session_index.sessions[i].session_id == session_id) {
            return &session_index.sessions[i];
        }
    }
    return NULL;
}

// Delete session
bool sd_manager_delete_session(uint16_t session_id) {
    // Remove from index
    for(uint16_t i = 0; i < session_index.count; i++) {
        if(session_index.sessions[i].session_id == session_id) {
            // Shift remaining sessions
            for(uint16_t j = i; j < session_index.count - 1; j++) {
                session_index.sessions[j] = session_index.sessions[j + 1];
            }
            session_index.count--;
            break;
        }
    }
    
    // Delete directory (would need recursive delete)
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_%03d", CAPTURES_PATH, session_id);
    storage_simply_remove_recursive(storage, path);
    
    sd_manager_save_session_index();
    
    FURI_LOG_I(TAG, "Deleted session %d", session_id);
    return true;
}

// Export session data
bool sd_manager_export_session(uint16_t session_id, ExportFormat_t format, const char* filename) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", EXPORTS_PATH, filename);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_EXPORT, true);
    if(!file) return false;
    
    SessionInfo_t* info = sd_manager_get_session(session_id);
    if(!info) {
        sd_manager_close_file(file);
        return false;
    }
    
    switch(format) {
        case EXPORT_FORMAT_CSV:
            // Write CSV header
            sd_manager_write_string(file, "timestamp,frequency_hz,rssi_dbm,data_hex\n");
            // Would write frame data here
            break;
            
        case EXPORT_FORMAT_JSON:
            sd_manager_write_string(file, "{\n  \"session\": {\n");
            // Would write JSON data here
            sd_manager_write_string(file, "  }\n}\n");
            break;
            
        case EXPORT_FORMAT_TEXT:
            sd_manager_write_string(file, "Flipper RF Lab Export\n");
            sd_manager_write_string(file, "==========================\n\n");
            // Would write formatted text here
            break;
            
        default:
            break;
    }
    
    sd_manager_close_file(file);
    FURI_LOG_I(TAG, "Exported session %d to %s", session_id, filename);
    
    return true;
}

// Export fingerprint
bool sd_manager_export_fingerprint(const RFFingerprint_t* fingerprint, const char* device_name) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.fp", FINGERPRINTS_PATH, device_name);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_FINGERPRINT, true);
    if(!file) return false;
    
    // Write binary fingerprint data
    sd_manager_write(file, (const uint8_t*)fingerprint, sizeof(RFFingerprint_t));
    
    // Write device name
    sd_manager_write_string(file, device_name);
    
    sd_manager_close_file(file);
    
    FURI_LOG_I(TAG, "Exported fingerprint: %s", device_name);
    return true;
}

// Export telemetry
bool sd_manager_export_telemetry(const SystemTelemetry_t* telemetry, const char* filename) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", LOGS_PATH, filename);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_LOG, true);
    if(!file) return false;
    
    char line[256];
    snprintf(line, sizeof(line),
        "%lu,%lu,%lu,%lu,%lu,%.2f\n",
        telemetry->timestamp,
        telemetry->cpu_load_percent,
        telemetry->frames_per_second,
        telemetry->buffer_utilization,
        telemetry->isr_latency_max_us,
        telemetry->battery_voltage
    );
    
    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    
    return true;
}

// Load configuration
bool sd_manager_load_config(RFConfig_t* config) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/settings.ini", CONFIG_PATH);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, false);
    if(!file) return false;
    
    // Would parse INI file format
    // For now, just close and return success
    
    sd_manager_close_file(file);
    return true;
}

// Save configuration
bool sd_manager_save_config(const RFConfig_t* config) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/settings.ini", CONFIG_PATH);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, true);
    if(!file) return false;
    
    char ini[512];
    snprintf(ini, sizeof(ini),
        "[RF]\n"
        "frequency=%lu\n"
        "data_rate=%lu\n"
        "modulation=%d\n"
        "tx_power=%d\n"
        "\n[Display]\n"
        "brightness=100\n"
        "contrast=50\n",
        config->frequency_hz,
        config->data_rate_baud,
        config->modulation,
        config->tx_power_dbm
    );
    
    sd_manager_write_string(file, ini);
    sd_manager_close_file(file);
    
    return true;
}

// Log event
bool sd_manager_log_event(const char* event, const char* details) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/system.log", LOGS_PATH);
    
    // Open in append mode
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_LOG, true);
    if(!file) return false;
    
    // Seek to end
    storage_file_seek(file->file, 0, true);
    
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    
    char line[256];
    snprintf(line, sizeof(line),
        "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s\n",
        datetime.year, datetime.month, datetime.day,
        datetime.hour, datetime.minute, datetime.second,
        event, details
    );
    
    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    
    return true;
}

// Log system status
bool sd_manager_log_system_status(const SystemTelemetry_t* telemetry) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/telemetry.csv", LOGS_PATH);
    
    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_LOG, true);
    if(!file) return false;
    
    // Check if file is new (write header)
    if(storage_file_tell(file->file) == 0) {
        sd_manager_write_string(file, "timestamp,cpu_load,frames_per_sec,buffer_util,isr_latency_ms,battery_v\n");
    }
    
    // Seek to end
    storage_file_seek(file->file, 0, true);
    
    char line[128];
    snprintf(line, sizeof(line),
        "%lu,%lu,%lu,%lu,%lu,%.2f\n",
        telemetry->uptime_seconds,
        telemetry->cpu_load_percent,
        telemetry->frames_per_second,
        telemetry->buffer_utilization,
        telemetry->isr_latency_max_us,
        telemetry->battery_voltage
    );
    
    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    
    return true;
}

// Get free space
uint64_t sd_manager_get_free_space(void) {
    if(!storage) return 0;
    
    uint64_t free_space;
    uint64_t total_space;
    
    storage_get_stats(storage, &free_space, &total_space);
    return free_space;
}

// Get total space
uint64_t sd_manager_get_total_space(void) {
    if(!storage) return 0;
    
    uint64_t free_space;
    uint64_t total_space;
    
    storage_get_stats(storage, &free_space, &total_space);
    return total_space;
}

// Check if enough space available
bool sd_manager_check_space(uint64_t required_bytes) {
    uint64_t free = sd_manager_get_free_space();
    return (free >= required_bytes);
}

// Format path
void sd_manager_format_path(char* out, size_t out_size, const char* base, const char* filename) {
    snprintf(out, out_size, "%s/%s", base, filename);
}

// Initialize rolling log
bool sd_manager_init_rolling_log(uint32_t max_size_mb) {
    rolling_log_max_size = max_size_mb * 1024 * 1024;
    
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/rolling.bin", LOGS_PATH);
    
    rolling_log_file = storage_file_alloc(storage);
    if(!storage_file_open(rolling_log_file, path, 
                          FS_ACCESS_MODE_WRITE, 
                          FS_OPEN_MODE_WRITE | FS_OPEN_MODE_CREATE_ALWAYS)) {
        storage_file_free(rolling_log_file);
        rolling_log_file = NULL;
        return false;
    }
    
    rolling_log_size = 0;
    return true;
}

// Write to rolling log
bool sd_manager_write_rolling_log(const uint8_t* data, uint32_t len) {
    if(!rolling_log_file) return false;
    
    // Check if we need to roll over
    if(rolling_log_size + len > rolling_log_max_size) {
        // Roll over - seek to beginning
        storage_file_seek(rolling_log_file, 0, false);
        rolling_log_size = 0;
    }
    
    uint32_t written = storage_file_write(rolling_log_file, data, len);
    rolling_log_size += written;
    
    return (written == len);
}

// Flush rolling log
void sd_manager_flush_rolling_log(void) {
    if(rolling_log_file) {
        storage_file_sync(rolling_log_file);
    }
}

// File enumeration
bool sd_manager_enum_files(const char* directory, FileEnumCallback callback, void* context) {
    if(!storage) return false;
    
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, directory)) {
        storage_file_free(dir);
        return false;
    }
    
    char name[MAX_FILENAME_LEN];
    while(storage_dir_read(dir, name, MAX_FILENAME_LEN)) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", directory, name);
        
        FileInfo info;
        if(storage_common_stat(storage, path, &info) == FSE_OK) {
            callback(path, info.size, context);
        }
    }
    
    storage_dir_close(dir);
    storage_file_free(dir);
    
    return true;
}

// Write compressed data (placeholder - would integrate with compression.c)
bool sd_manager_write_compressed(FileHandle_t* handle, const uint8_t* data, uint32_t len) {
    // For now, just write uncompressed
    // Full implementation would compress data before writing
    return sd_manager_write(handle, data, len);
}

// Read compressed data (placeholder)
bool sd_manager_read_compressed(FileHandle_t* handle, uint8_t* data, uint32_t max_len, uint32_t* out_len) {
    // For now, just read uncompressed
    // Full implementation would decompress after reading
    *out_len = max_len;
    return sd_manager_read(handle, data, max_len);
}
