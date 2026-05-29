#include "sd_manager.h"
#include <datetime/datetime.h>

#define TAG "SD_MGR"

static Storage* storage            = NULL;
static SessionIndex_t session_index;
static bool     sd_initialized     = false;
static uint32_t rolling_log_size   = 0;
static uint32_t rolling_log_max_size = 0;
static File*    rolling_log_file   = NULL;

// FIX: static file-handle pool — no malloc after init
#define MAX_OPEN_FILES 4
static FileHandle_t file_handle_pool[MAX_OPEN_FILES];
static bool         file_handle_in_use[MAX_OPEN_FILES] = {false};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static FileHandle_t* _pool_acquire(void) {
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!file_handle_in_use[i]) {
            file_handle_in_use[i] = true;
            return &file_handle_pool[i];
        }
    }
    FURI_LOG_E(TAG, "No free file handles");
    return NULL;
}

static void _pool_release(FileHandle_t* handle) {
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(&file_handle_pool[i] == handle) {
            file_handle_in_use[i] = false;
            return;
        }
    }
}

// ============================================================================
// INIT / DEINIT
// ============================================================================

FuriStatus sd_manager_init(void) {
    if(sd_initialized) return FuriStatusOk;

    FURI_LOG_I(TAG, "Initializing SD manager");

    storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return FuriStatusError;
    }

    if(!sd_manager_is_card_present()) {
        FURI_LOG_W(TAG, "SD card not present");
        furi_record_close(RECORD_STORAGE);
        return FuriStatusError;
    }

    if(!sd_manager_ensure_directories()) {
        FURI_LOG_E(TAG, "Failed to create directory structure");
        furi_record_close(RECORD_STORAGE);
        return FuriStatusError;
    }

    sd_manager_load_session_index();

    sd_initialized = true;
    FURI_LOG_I(TAG, "SD manager initialized, %d sessions found", session_index.count);
    return FuriStatusOk;
}

void sd_manager_deinit(void) {
    if(!sd_initialized) return;

    if(rolling_log_file) {
        storage_file_close(rolling_log_file);
        storage_file_free(rolling_log_file);
        rolling_log_file = NULL;
    }

    sd_manager_save_session_index();
    furi_record_close(RECORD_STORAGE);
    storage = NULL;
    sd_initialized = false;
    FURI_LOG_I(TAG, "SD manager deinitialized");
}

bool sd_manager_is_card_present(void) {
    if(!storage) return false;
    return storage_sd_status(storage) == FSDStatusOK;
}

bool sd_manager_ensure_directories(void) {
    if(!storage) return false;
    const char* dirs[] = {
        SD_BASE_PATH, CAPTURES_PATH, FINGERPRINTS_PATH,
        LOGS_PATH, EXPORTS_PATH, CONFIG_PATH
    };
    for(uint8_t i = 0; i < 6; i++) {
        if(!storage_simply_mkdir(storage, dirs[i]))
            FURI_LOG_W(TAG, "mkdir (may already exist): %s", dirs[i]);
    }
    return true;
}

// ============================================================================
// FILE HANDLE POOL
// ============================================================================

FileHandle_t* sd_manager_open_file(const char* path, FileType_t type, bool write) {
    if(!storage) return NULL;

    FileHandle_t* handle = _pool_acquire();
    if(!handle) return NULL;

    handle->storage       = storage;
    handle->file          = storage_file_alloc(storage);
    handle->type          = type;
    handle->is_open       = false;
    handle->bytes_written = 0;
    handle->bytes_read    = 0;
    handle->open_time     = furi_get_tick();

    strncpy(handle->path, path, MAX_PATH_LEN - 1);
    handle->path[MAX_PATH_LEN - 1] = '\0';

    FS_AccessMode access    = write ? FS_ACCESS_MODE_WRITE  : FS_ACCESS_MODE_READ;
    // FIX (major): write mode now uses CREATE_ALWAYS only for true overwrites.
    //   Append callers must use sd_manager_open_file_append().
    FS_OpenMode   open_mode = write
        ? (FS_OPEN_MODE_WRITE | FS_OPEN_MODE_CREATE_ALWAYS)
        : FS_OPEN_MODE_READ;

    if(!storage_file_open(handle->file, path, access, open_mode)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
        storage_file_free(handle->file);
        _pool_release(handle);
        return NULL;
    }

    handle->is_open = true;
    return handle;
}

// FIX (major): Append variant — opens or creates, always seeks to end.
//   Used by log_event() and log_system_status() so entries accumulate.
FileHandle_t* sd_manager_open_file_append(const char* path, FileType_t type) {
    if(!storage) return NULL;

    FileHandle_t* handle = _pool_acquire();
    if(!handle) return NULL;

    handle->storage       = storage;
    handle->file          = storage_file_alloc(storage);
    handle->type          = type;
    handle->is_open       = false;
    handle->bytes_written = 0;
    handle->bytes_read    = 0;
    handle->open_time     = furi_get_tick();

    strncpy(handle->path, path, MAX_PATH_LEN - 1);
    handle->path[MAX_PATH_LEN - 1] = '\0';

    if(!storage_file_open(handle->file, path,
                          FS_ACCESS_MODE_WRITE,
                          FS_OPEN_MODE_WRITE | FS_OPEN_MODE_CREATE_NEW_IF_NOT_EXISTS)) {
        storage_file_free(handle->file);
        _pool_release(handle);
        return NULL;
    }
    // Seek to end so new data appends
    storage_file_seek(handle->file, 0, false); // false = from end
    handle->is_open = true;
    return handle;
}

void sd_manager_close_file(FileHandle_t* handle) {
    if(!handle) return;
    if(handle->is_open && handle->file) {
        storage_file_close(handle->file);
        storage_file_free(handle->file);
    }
    uint32_t duration = furi_get_tick() - handle->open_time;
    FURI_LOG_D(TAG, "File closed: %s (%lu ms)", handle->path, duration);
    _pool_release(handle);
}

bool sd_manager_write(FileHandle_t* handle, const uint8_t* data, uint32_t len) {
    if(!handle || !handle->is_open) return false;
    uint32_t written = storage_file_write(handle->file, data, len);
    if(written != len) {
        FURI_LOG_E(TAG, "Write failed: %lu of %lu bytes", written, len);
        return false;
    }
    handle->bytes_written += written;
    return true;
}

bool sd_manager_read(FileHandle_t* handle, uint8_t* data, uint32_t len) {
    if(!handle || !handle->is_open) return false;
    uint32_t read = storage_file_read(handle->file, data, len);
    handle->bytes_read += read;
    return (read == len);
}

bool sd_manager_write_string(FileHandle_t* handle, const char* str) {
    return sd_manager_write(handle, (const uint8_t*)str, strlen(str));
}

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

bool sd_manager_create_session_directory(uint16_t session_id, char* path_out, size_t path_size) {
    snprintf(path_out, path_size, "%s/session_%03d", CAPTURES_PATH, session_id);
    if(!storage_simply_mkdir(storage, path_out)) {
        FURI_LOG_E(TAG, "Failed to create session directory: %s", path_out);
        return false;
    }
    char subdir[MAX_PATH_LEN];
    snprintf(subdir, sizeof(subdir), "%s/raw", path_out);
    storage_simply_mkdir(storage, subdir);
    snprintf(subdir, sizeof(subdir), "%s/analyzed", path_out);
    storage_simply_mkdir(storage, subdir);
    return true;
}

uint16_t sd_manager_create_session(const char* name) {
    if(session_index.count >= MAX_SESSIONS) {
        FURI_LOG_E(TAG, "Maximum sessions (%d) reached", MAX_SESSIONS);
        return 0;
    }

    uint16_t session_id = session_index.count + 1;
    SessionInfo_t* info = &session_index.sessions[session_index.count];

    // Initialise magic/version on first create — not in SessionInfo_t,
    // but ensure the index header is stamped before first save.
    session_index.magic   = SESSION_INDEX_MAGIC;
    session_index.version = SESSION_INDEX_VERSION;
    info->session_id = session_id;
    strncpy(info->session_name, name, SESSION_NAME_LEN - 1);
    info->session_name[SESSION_NAME_LEN - 1] = '\0';

    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    snprintf(info->timestamp, sizeof(info->timestamp),
             "%04d-%02d-%02d_%02d-%02d-%02d",
             datetime.year, datetime.month, datetime.day,
             datetime.hour, datetime.minute, datetime.second);

    // FIX (major): record tick at session open so duration can be computed
    info->start_tick     = furi_get_tick();
    info->num_frames     = 0;
    info->duration_ms    = 0;
    info->file_size      = 0;
    info->has_raw        = false;
    info->has_analyzed   = false;
    info->has_metadata   = false;

    char path[MAX_PATH_LEN];
    if(!sd_manager_create_session_directory(session_id, path, sizeof(path)))
        return 0;

    session_index.count++;
    session_index.current_session = session_id;

    FURI_LOG_I(TAG, "Created session %d: %s", session_id, name);
    return session_id;
}

bool sd_manager_close_session(uint16_t session_id) {
    SessionInfo_t* info = sd_manager_get_session(session_id);
    if(!info) return false;

    // FIX (major): compute actual elapsed time from the recorded start tick
    info->duration_ms = furi_get_tick() - info->start_tick;

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
            info->has_raw      ? "true" : "false",
            info->has_analyzed ? "true" : "false");

        sd_manager_write_string(file, metadata);
        sd_manager_close_file(file);
        info->has_metadata = true;
    }

    FURI_LOG_I(TAG, "Closed session %d (%lu ms)", session_id, info->duration_ms);
    return true;
}

bool sd_manager_load_session_index(void) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_index.bin", SD_BASE_PATH);

    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, false);
    if(!file) {
        memset(&session_index, 0, sizeof(session_index));
        return true;
    }

    bool success = sd_manager_read(file, (uint8_t*)&session_index, sizeof(session_index));
    sd_manager_close_file(file);

    if(!success) {
        memset(&session_index, 0, sizeof(session_index));
        return false;
    }
    // FIX (note): schema change guard — wipe and start fresh if version mismatch.
    if(session_index.magic != SESSION_INDEX_MAGIC ||
       session_index.version != SESSION_INDEX_VERSION) {
        FURI_LOG_W(TAG, "Session index schema mismatch — resetting");
        memset(&session_index, 0, sizeof(session_index));
        session_index.magic   = SESSION_INDEX_MAGIC;
        session_index.version = SESSION_INDEX_VERSION;
    }
    return true;
}

bool sd_manager_save_session_index(void) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_index.bin", SD_BASE_PATH);

    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_CONFIG, true);
    if(!file) return false;

    bool success = sd_manager_write(file, (const uint8_t*)&session_index, sizeof(session_index));
    sd_manager_close_file(file);
    return success;
}

SessionInfo_t* sd_manager_get_session(uint16_t session_id) {
    for(uint16_t i = 0; i < session_index.count; i++) {
        if(session_index.sessions[i].session_id == session_id)
            return &session_index.sessions[i];
    }
    return NULL;
}

bool sd_manager_delete_session(uint16_t session_id) {
    for(uint16_t i = 0; i < session_index.count; i++) {
        if(session_index.sessions[i].session_id == session_id) {
            for(uint16_t j = i; j < session_index.count - 1; j++)
                session_index.sessions[j] = session_index.sessions[j + 1];
            session_index.count--;
            break;
        }
    }

    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/session_%03d", CAPTURES_PATH, session_id);
    storage_simply_remove_recursive(storage, path);

    sd_manager_save_session_index();
    FURI_LOG_I(TAG, "Deleted session %d", session_id);
    return true;
}

// ============================================================================
// EXPORT
// ============================================================================

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
            sd_manager_write_string(file, "timestamp,frequency_hz,rssi_dbm,data_hex\n");
            // TODO: iterate frames and write rows
            break;
        case EXPORT_FORMAT_JSON:
            sd_manager_write_string(file, "{\"session\": {}}\n");
            // TODO: serialise session frames
            break;
        case EXPORT_FORMAT_TEXT:
            sd_manager_write_string(file, "Flipper RF Lab Export\n");
            sd_manager_write_string(file, "==========================\n\n");
            // TODO: formatted text output
            break;
        default:
            break;
    }

    sd_manager_close_file(file);
    FURI_LOG_I(TAG, "Exported session %d to %s", session_id, filename);
    return true;
}

bool sd_manager_export_fingerprint(const RFFingerprint_t* fingerprint, const char* device_name) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.fp", FINGERPRINTS_PATH, device_name);

    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_FINGERPRINT, true);
    if(!file) return false;

    sd_manager_write(file, (const uint8_t*)fingerprint, sizeof(RFFingerprint_t));
    sd_manager_write_string(file, device_name);
    sd_manager_close_file(file);

    FURI_LOG_I(TAG, "Exported fingerprint: %s", device_name);
    return true;
}

// FIX (major): battery_voltage / timestamp fields did not exist in
//   SystemTelemetry_t.  Use battery_mv (uint16_t) and uptime_seconds.
//   FIX: no float formatting — Newlib-nano on Flipper lacks FPU printf.
//   Format voltage as integer millivolts.
bool sd_manager_export_telemetry(const SystemTelemetry_t* telemetry, const char* filename) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", LOGS_PATH, filename);

    FileHandle_t* file = sd_manager_open_file(path, FILE_TYPE_LOG, true);
    if(!file) return false;

    char line[256];
    // uptime_seconds, cpu_load, fps, buffer_util, isr_latency_us, battery_mv
    snprintf(line, sizeof(line),
        "%lu,%lu,%lu,%lu,%lu,%u\n",
        telemetry->uptime_seconds,
        telemetry->cpu_load_percent,
        telemetry->frames_per_second,
        telemetry->buffer_utilization,
        telemetry->isr_latency_max_us,
        (unsigned)telemetry->battery_mv);

    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    return true;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool sd_manager_load_config(RFConfig_t* config) {
    UNUSED(config);
    // TODO: parse settings.ini and populate config fields
    FURI_LOG_W(TAG, "sd_manager_load_config: NOT IMPLEMENTED — returning false");
    return false;
}

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
        config->tx_power_dbm);

    sd_manager_write_string(file, ini);
    sd_manager_close_file(file);
    return true;
}

// ============================================================================
// LOGGING
// ============================================================================

// FIX (major): now uses sd_manager_open_file_append() so entries accumulate
//   rather than truncating on every call.
bool sd_manager_log_event(const char* event, const char* details) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/system.log", LOGS_PATH);

    FileHandle_t* file = sd_manager_open_file_append(path, FILE_TYPE_LOG);
    if(!file) return false;

    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    char line[256];
    snprintf(line, sizeof(line),
        "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s\n",
        datetime.year, datetime.month, datetime.day,
        datetime.hour, datetime.minute, datetime.second,
        event, details);

    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    return true;
}

// FIX (major): append mode + fixed field names + no float formatting
bool sd_manager_log_system_status(const SystemTelemetry_t* telemetry) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/telemetry.csv", LOGS_PATH);

    FileHandle_t* file = sd_manager_open_file_append(path, FILE_TYPE_LOG);
    if(!file) return false;

    // Write header only if file was just created (size == 0 after open)
    if(storage_file_tell(file->file) == 0) {
        sd_manager_write_string(file,
            "uptime_s,cpu_load,frames_per_sec,buffer_util,isr_latency_us,battery_mv\n");
    }

    char line[128];
    snprintf(line, sizeof(line),
        "%lu,%lu,%lu,%lu,%lu,%u\n",
        telemetry->uptime_seconds,
        telemetry->cpu_load_percent,
        telemetry->frames_per_second,
        telemetry->buffer_utilization,
        telemetry->isr_latency_max_us,
        (unsigned)telemetry->battery_mv);

    sd_manager_write_string(file, line);
    sd_manager_close_file(file);
    return true;
}

// ============================================================================
// SPACE UTILITIES
// ============================================================================

uint64_t sd_manager_get_free_space(void) {
    if(!storage) return 0;
    uint64_t free_space, total_space;
    storage_get_stats(storage, &free_space, &total_space);
    return free_space;
}

uint64_t sd_manager_get_total_space(void) {
    if(!storage) return 0;
    uint64_t free_space, total_space;
    storage_get_stats(storage, &free_space, &total_space);
    return total_space;
}

bool sd_manager_check_space(uint64_t required_bytes) {
    return sd_manager_get_free_space() >= required_bytes;
}

void sd_manager_format_path(char* out, size_t out_size, const char* base, const char* filename) {
    snprintf(out, out_size, "%s/%s", base, filename);
}

// ============================================================================
// ROLLING LOG
// ============================================================================

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

// FIX (major): on rollover, reopen with CREATE_ALWAYS to produce a clean
//   file rather than overwriting the start of the old one in-place
//   (which left stale data at the tail).
bool sd_manager_write_rolling_log(const uint8_t* data, uint32_t len) {
    if(!rolling_log_file) return false;

    if(rolling_log_size + len > rolling_log_max_size) {
        // Roll over: close, reopen truncated
        storage_file_close(rolling_log_file);
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/rolling.bin", LOGS_PATH);
        if(!storage_file_open(rolling_log_file, path,
                              FS_ACCESS_MODE_WRITE,
                              FS_OPEN_MODE_WRITE | FS_OPEN_MODE_CREATE_ALWAYS)) {
            storage_file_free(rolling_log_file);
            rolling_log_file = NULL;
            return false;
        }
        rolling_log_size = 0;
    }

    uint32_t written = storage_file_write(rolling_log_file, data, len);
    rolling_log_size += written;
    return (written == len);
}

void sd_manager_flush_rolling_log(void) {
    if(rolling_log_file) storage_file_sync(rolling_log_file);
}

// ============================================================================
// FILE ENUMERATION
// ============================================================================

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
        if(storage_common_stat(storage, path, &info) == FSE_OK)
            callback(path, info.size, context);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return true;
}

// ============================================================================
// COMPRESSION SHIMS (stubs — integration with compression.c pending)
// ============================================================================

bool sd_manager_write_compressed(FileHandle_t* handle, const uint8_t* data, uint32_t len) {
    // TODO: compress data before writing
    return sd_manager_write(handle, data, len);
}

bool sd_manager_read_compressed(FileHandle_t* handle, uint8_t* data, uint32_t max_len, uint32_t* out_len) {
    // TODO: decompress after reading
    *out_len = max_len;
    return sd_manager_read(handle, data, max_len);
}
