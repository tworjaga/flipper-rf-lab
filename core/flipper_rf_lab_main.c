#include "flipper_rf_lab.h"
#include "hal/cc1101_driver.h"
#include "hal/gpio_manager.h"
#include "hal/timer_precision.h"
#include "math/fixed_point.h"
#include "math/statistics.h"
#include "storage/sd_manager.h"
#include "storage/compression.h"
#include "analysis/fingerprinting.h"
#include "analysis/clustering.h"
#include "analysis/threat_model.h"
#include "analysis/protocol_infer.h"
#include "ui/main_menu.h"
#include "research/telemetry.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <furi_hal_power.h>
#include <string.h>

#define TAG "FlipperRFLab"

// Statically allocated buffers - NO dynamic allocation after init
static uint8_t dma_buffer[SPI_DMA_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t pulse_buffer[PULSE_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t frame_buffer[FRAME_BUFFER_SIZE] __attribute__((aligned(4)));

// Platform context - single global instance
static FlipperRFLabContext platform_context = {0};

// FreeRTOS task handles
static FuriThread* rf_capture_thread = NULL;
static FuriThread* ui_update_thread = NULL;
static FuriThread* analysis_thread = NULL;

// GUI handles
static Gui* gui = NULL;
static ViewDispatcher* view_dispatcher = NULL;
static NotificationApp* notifications = NULL;

// Shutdown flag (FIX 1)
volatile bool g_shutdown_requested = false;

// Function prototypes (kept static, local to this TU — not in public header)
static int32_t rf_capture_worker(void* context);
static int32_t ui_update_worker(void* context);
static int32_t analysis_worker(void* context);
static void update_system_telemetry(void);
static void enter_low_power_mode(void);
static bool app_navigation_callback(void* context);

// Count of captured frames awaiting a heavier analysis pass.
static volatile uint32_t g_frames_pending = 0;

// ============================================================================
// INITIALIZATION
// ============================================================================

static bool flipper_rf_lab_init(void) {
    FURI_LOG_I(TAG, "Flipper RF Lab v1.0.0 Starting...");
    
    // Initialize memory pools (static allocation only)
    platform_context.dma_buffer = dma_buffer;
    platform_context.dma_buffer_size = SPI_DMA_BUFFER_SIZE;
    platform_context.pulse_buffer = pulse_buffer;
    platform_context.pulse_buffer_size = PULSE_BUFFER_SIZE;
    platform_context.frame_buffer = frame_buffer;
    platform_context.frame_buffer_size = FRAME_BUFFER_SIZE;
    
    // Initialize circular buffers
    circular_buffer_init(&platform_context.rx_buffer, dma_buffer, SPI_DMA_BUFFER_SIZE);
    circular_buffer_init(&platform_context.pulse_circ_buffer, pulse_buffer, PULSE_BUFFER_SIZE);

    // Static bump allocator (init-time objects only)
    memory_pools_init();

    // Initialize precision timing (DWT cycle counter) first — other init paths
    // use timer_get_us()/delays.
    timer_precision_init();

    // FIX (critical): configure spare GPIO BEFORE bringing up the CC1101/SPI so
    // the GPIO manager can never clobber the SPI/CS/GDO pins afterwards.
    if(gpio_manager_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "GPIO manager initialization failed");
        return false;
    }

    // Initialize hardware abstraction layer
    if(cc1101_driver_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "CC1101 driver initialization failed");
        // Non-fatal: an external CC1101 module may not be attached. The UI and
        // offline analysis still work; capture simply yields no data.
        FURI_LOG_W(TAG, "Continuing without CC1101 (no module detected)");
    }

    // Initialize fixed-point math library
    fixed_point_init();

    // Initialize storage subsystem
    if(sd_manager_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "SD manager initialization failed");
        // Non-fatal - can still operate without SD
    }

    // Initialize analysis / support engines
    compression_init();
    protocol_infer_init();
    telemetry_init();

    if(fingerprinting_engine_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "Fingerprinting engine initialization failed");
        return false;
    }
    
    if(clustering_engine_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "Clustering engine initialization failed");
        return false;
    }
    
    if(threat_model_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "Threat model initialization failed");
        return false;
    }
    
    // Initialize GUI
    gui = furi_record_open(RECORD_GUI);
    view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    
    // Initialize main menu
    main_menu_init(view_dispatcher);

    // FIX (critical): give the user a way out. The Back button from the root
    // menu runs this navigation callback; returning false tells the dispatcher
    // to stop, which unblocks view_dispatcher_run() in main().
    view_dispatcher_set_event_callback_context(view_dispatcher, &platform_context);
    view_dispatcher_set_navigation_event_callback(view_dispatcher, app_navigation_callback);

    // Get notification service
    notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Create worker threads
    rf_capture_thread = furi_thread_alloc();
    furi_thread_set_name(rf_capture_thread, "RF_Capture");
    furi_thread_set_stack_size(rf_capture_thread, RF_CAPTURE_STACK_SIZE);
    furi_thread_set_callback(rf_capture_thread, rf_capture_worker);
    furi_thread_set_context(rf_capture_thread, &platform_context);
    
    ui_update_thread = furi_thread_alloc();
    furi_thread_set_name(ui_update_thread, "UI_Update");
    furi_thread_set_stack_size(ui_update_thread, UI_UPDATE_STACK_SIZE);
    furi_thread_set_callback(ui_update_thread, ui_update_worker);
    furi_thread_set_context(ui_update_thread, &platform_context);
    
    analysis_thread = furi_thread_alloc();
    furi_thread_set_name(analysis_thread, "Analysis");
    furi_thread_set_stack_size(analysis_thread, ANALYSIS_STACK_SIZE);
    furi_thread_set_callback(analysis_thread, analysis_worker);
    furi_thread_set_context(analysis_thread, &platform_context);
    
    FURI_LOG_I(TAG, "Flipper RF Lab initialized successfully");
    return true;
}

// ============================================================================
// WORKER THREADS
// ============================================================================

static int32_t rf_capture_worker(void* context) {
    FlipperRFLabContext* ctx = (FlipperRFLabContext*)context;
    UNUSED(ctx);
    
    FURI_LOG_I(TAG, "RF capture worker started");
    
    while(!g_shutdown_requested) {
        // Service pending RSSI sample from ISR (FIX 2)
        cc1101_service_rssi_pending();

        // Check for RF activity
        if(cc1101_has_data()) {
            capture_frame_burst();
        }
        
        // Spectrum sweep mode
        if(ctx->rf_config.band == BAND_CUSTOM) {
            spectrum_sweep_step();
        }
        
        // Passive monitoring mode
        if(ctx->low_power_mode) {
            passive_monitor_cycle();
        }

        // Yield to the scheduler (FIX: furi_delay_us is a busy-wait that pinned
        // the core at 100%; furi_delay_ms sleeps the thread).
        furi_delay_ms(2);
    }

    return 0;
}

static int32_t ui_update_worker(void* context) {
    FlipperRFLabContext* ctx = (FlipperRFLabContext*)context;
    UNUSED(ctx);
    
    FURI_LOG_I(TAG, "UI update worker started");
    
    uint32_t last_update = furi_get_tick();
    
    while(!g_shutdown_requested) {
        uint32_t now = furi_get_tick();

        // Update display at ~30fps (33ms interval)
        if(now - last_update >= 33) {
            update_display();
            last_update = now;
        }

        furi_delay_ms(5);   // FIX: sleep instead of busy-wait
    }

    return 0;
}

static int32_t analysis_worker(void* context) {
    FlipperRFLabContext* ctx = (FlipperRFLabContext*)context;
    UNUSED(ctx);
    
    FURI_LOG_I(TAG, "Analysis worker started");
    
    while(!g_shutdown_requested) {
        // FIX: enter low-power at most once (was re-firing every ~100us,
        // spamming SPI writes, logs and notifications). battery_mv==0 means
        // "not yet sampled", so don't act on it.
        if(platform_context.telemetry.battery_mv > 0 &&
           platform_context.telemetry.battery_mv < 3300 &&
           !platform_context.low_power_mode) {
            enter_low_power_mode();
        }

        if(has_pending_analysis()) {
            process_next_analysis_task();
        }

        // Update telemetry periodically
        static uint32_t last_telemetry = 0;
        uint32_t now = furi_get_tick();
        if(now - last_telemetry >= 1000) {  // Every second
            update_system_telemetry();
            last_telemetry = now;
        }

        furi_delay_ms(5);   // FIX: sleep instead of busy-wait
    }

    return 0;
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

static void update_system_telemetry(void) {
    FlipperRFLabContext* ctx = &platform_context;

    // Approximate CPU-active cycles since the last window.
    // FIX (critical): reset the *active*-cycle accumulator, NOT DWT_CYCCNT.
    // Zeroing DWT_CYCCNT broke furi's own microsecond delays and HAL timeouts,
    // which read the same free-running counter.
    uint32_t active_cycles = dwt_get_active_cycles();
    uint32_t total_cycles = SYSTEM_CORE_CLOCK;  // ~1 s window
    if(total_cycles > 0) {
        uint32_t load = (uint32_t)(((uint64_t)active_cycles * 100) / total_cycles);
        ctx->telemetry.cpu_load_percent = (load > 100) ? 100 : load;
    }
    dwt_reset_active_cycle_counter();

    // Update buffer utilization
    ctx->telemetry.buffer_utilization =
        (circular_buffer_count(&ctx->rx_buffer) * 100) / SPI_DMA_BUFFER_SIZE;

    // Sample battery voltage (mV) so the low-power check has real data.
    float vbat = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    ctx->telemetry.battery_mv = (uint16_t)(vbat * 1000.0f);

    // Update uptime
    ctx->telemetry.uptime_seconds = furi_get_tick() / 1000;
    
    // Log telemetry if in debug mode
    FURI_LOG_D(TAG, "CPU: %lu%%, Buffer: %lu%%, Uptime: %lu s",
               ctx->telemetry.cpu_load_percent,
               ctx->telemetry.buffer_utilization,
               ctx->telemetry.uptime_seconds);
}

static void enter_low_power_mode(void) {
    FURI_LOG_I(TAG, "Entering low power mode");
    
    // Reduce CC1101 duty cycle
    cc1101_set_low_power_mode(true);
    
    // Slow down non-critical tasks
    platform_context.low_power_mode = true;
    
    // Notify user
    notification_message(notifications, &sequence_blink_blue_100);
}

// ============================================================================
// CAPTURE / ANALYSIS / DISPLAY RUNTIME
// (declared in flipper_rf_lab.h; previously unimplemented — the app could not
//  link into firmware because these symbols were missing.)
// ============================================================================

// Pull one packet from the CC1101 RX FIFO (if any) into the current session
// and feed the analysis engines. Safe no-op when no module/data is present.
void capture_frame_burst(void) {
    uint8_t data[64];
    uint8_t len = 0, rssi = 0, lqi = 0;

    if(!cc1101_receive_packet(data, &len, &rssi, &lqi) || len == 0) {
        return;
    }

    Session_t* s = &platform_context.current_session;
    Frame_t* f = &s->frames[s->current_idx];
    memset(f, 0, sizeof(*f));

    uint8_t copy_len = (len > sizeof(f->data)) ? sizeof(f->data) : len;
    memcpy(f->data, data, copy_len);
    f->length       = copy_len;
    f->timestamp_us = timer_get_us();
    f->rssi_dbm     = cc1101_rssi_to_dbm(rssi);
    f->frequency_hz = platform_context.rf_config.frequency_hz;

    s->current_idx = (s->current_idx + 1) % MAX_FRAME_COUNT;
    if(s->count < MAX_FRAME_COUNT) s->count++;
    platform_context.total_frames_processed++;

    // Feed the online analysis engines (all bounded, non-blocking).
    fingerprinting_process_frame(f);
    protocol_infer_add_frame(f);
    threat_model_add_frame(f);

    __atomic_fetch_add(&g_frames_pending, 1, __ATOMIC_RELAXED);
}

// Advance the spectrum sweep by one dwell step across 300–928 MHz.
void spectrum_sweep_step(void) {
    static uint32_t freq_hz = 300000000UL;
    cc1101_set_frequency(freq_hz);
    (void)cc1101_read_rssi();   // sample occupancy at this step
    freq_hz += 1000000UL;       // 1 MHz steps
    if(freq_hz > 928000000UL) freq_hz = 300000000UL;
}

// Lightweight passive-monitor tick: fold in the latest ISR-serviced RSSI.
void passive_monitor_cycle(void) {
    uint8_t rssi = cc1101_get_rssi_sample();
    fingerprinting_process_rssi_sample(rssi, timer_get_us());
}

bool has_pending_analysis(void) {
    return __atomic_load_n(&g_frames_pending, __ATOMIC_RELAXED) > 0;
}

// Consume one pending frame's worth of deferred analysis. The per-frame online
// work already ran in capture_frame_burst(); this is where a heavier periodic
// pass can run without blocking capture.
void process_next_analysis_task(void) {
    if(__atomic_load_n(&g_frames_pending, __ATOMIC_RELAXED) == 0) return;
    __atomic_fetch_sub(&g_frames_pending, 1, __ATOMIC_RELAXED);
}

// The submenu view redraws itself through the GUI; nothing to push here yet.
// Kept as the single, defined hook the UI worker calls at ~30fps.
void update_display(void) {
}

// Back from the root view. Returning false leaves the Back event unhandled,
// which tells the view dispatcher to stop — unblocking view_dispatcher_run()
// in main(). (Don't also call view_dispatcher_stop() here: returning false
// already initiates the stop, and doing both double-signals it.)
static bool app_navigation_callback(void* context) {
    UNUSED(context);
    return false;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int32_t flipper_rf_lab_main(void* p) {
    UNUSED(p);
    
    // Initialize platform
    if(!flipper_rf_lab_init()) {
        FURI_LOG_E(TAG, "Platform initialization failed");
        return -1;
    }
    
    // Start worker threads
    furi_thread_start(rf_capture_thread);
    furi_thread_start(ui_update_thread);
    furi_thread_start(analysis_thread);
    
    FURI_LOG_I(TAG, "All workers started, entering GUI event loop");

    // Blocks main thread until view_dispatcher_stop() is called from
    // a back-button / exit callback that also sets g_shutdown_requested = true.
    // (FIX 4)
    view_dispatcher_run(view_dispatcher);

    // view_dispatcher_run() returned — initiate shutdown (FIX 1+4+7)
    g_shutdown_requested = true;

    // Cleanup
    FURI_LOG_I(TAG, "Shutting down...");

    furi_thread_join(rf_capture_thread);
    furi_thread_free(rf_capture_thread);

    furi_thread_join(ui_update_thread);
    furi_thread_free(ui_update_thread);

    furi_thread_join(analysis_thread);
    furi_thread_free(analysis_thread);

    // FIX: tear down the menu (removes its view from the dispatcher and frees
    // the submenu) before freeing the dispatcher itself.
    main_menu_deinit();

    view_dispatcher_free(view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    sd_manager_deinit();
    cc1101_driver_deinit();
    
    FURI_LOG_I(TAG, "Shutdown complete");
    
    return 0;
}
