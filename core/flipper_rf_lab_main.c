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

// Function prototypes
static int32_t rf_capture_worker(void* context);
static int32_t ui_update_worker(void* context);
static int32_t analysis_worker(void* context);
static void update_system_telemetry(void);

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
    
    // Initialize hardware abstraction layer
    if(cc1101_driver_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "CC1101 driver initialization failed");
        return false;
    }
    
    if(gpio_manager_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "GPIO manager initialization failed");
        return false;
    }
    
    // Initialize precision timing (DWT cycle counter)
    timer_precision_init();
    
    // Initialize fixed-point math library
    fixed_point_init();
    
    // Initialize storage subsystem
    if(sd_manager_init() != FuriStatusOk) {
        FURI_LOG_E(TAG, "SD manager initialization failed");
        // Non-fatal - can still operate without SD
    }
    
    // Initialize analysis engines
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
    
    // Get notification service
    notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Create worker threads
    rf_capture_thread = furi_thread_alloc();
    furi_thread_set_name(rf_capture_thread, "RF_Capture");
    furi_thread_set_stack_size(rf_capture_thread, MAX_STACK_DEPTH);
    furi_thread_set_callback(rf_capture_thread, rf_capture_worker);
    furi_thread_set_context(rf_capture_thread, &platform_context);
    
    ui_update_thread = furi_thread_alloc();
    furi_thread_set_name(ui_update_thread, "UI_Update");
    furi_thread_set_stack_size(ui_update_thread, MAX_STACK_DEPTH);
    furi_thread_set_callback(ui_update_thread, ui_update_worker);
    furi_thread_set_context(ui_update_thread, &platform_context);
    
    analysis_thread = furi_thread_alloc();
    furi_thread_set_name(analysis_thread, "Analysis");
    furi_thread_set_stack_size(analysis_thread, MAX_STACK_DEPTH);
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
    
    while(1) {
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
        
        // Yield to other tasks
        furi_delay_us(100);
    }
    
    return 0;
}

static int32_t ui_update_worker(void* context) {
    FlipperRFLabContext* ctx = (FlipperRFLabContext*)context;
    UNUSED(ctx);
    
    FURI_LOG_I(TAG, "UI update worker started");
    
    uint32_t last_update = furi_get_tick();
    
    while(1) {
        uint32_t now = furi_get_tick();
        
        // Update display at 30fps (33ms interval)
        if(now - last_update >= 33) {
            update_display();
            last_update = now;
        }
        
        // Process user input
        view_dispatcher_run(view_dispatcher);
        
        furi_delay_us(1000);
    }
    
    return 0;
}

static int32_t analysis_worker(void* context) {
    FlipperRFLabContext* ctx = (FlipperRFLabContext*)context;
    UNUSED(ctx);
    
    FURI_LOG_I(TAG, "Analysis worker started");
    
    while(1) {
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
        
        furi_delay_us(100);
    }
    
    return 0;
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

static void update_system_telemetry(void) {
    FlipperRFLabContext* ctx = &platform_context;
    
    // Calculate CPU load using DWT cycle counter
    uint32_t active_cycles = dwt_get_active_cycles();
    uint32_t total_cycles = SYSTEM_CORE_CLOCK / 100;  // 10ms worth of cycles
    
    ctx->telemetry.cpu_load_percent = (active_cycles * 100) / total_cycles;
    if(ctx->telemetry.cpu_load_percent > 100) {
        ctx->telemetry.cpu_load_percent = 100;
    }
    
    // Reset cycle counter for next measurement
    dwt_reset_cycle_counter();
    
    // Update buffer utilization
    ctx->telemetry.buffer_utilization = 
        (circular_buffer_count(&ctx->rx_buffer) * 100) / SPI_DMA_BUFFER_SIZE;
    
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
    
    FURI_LOG_I(TAG, "All workers started, entering main loop");
    
    // Main loop - handle system state changes
    while(1) {
        // Check for shutdown signal
        if(platform_context.current_session.config.band == BAND_CUSTOM) {
            // Special case: custom band = shutdown request
            break;
        }
        
        // Enter low power if battery critical
        if(platform_context.telemetry.battery_voltage < 3.3f) {
            enter_low_power_mode();
        }
        
        furi_delay_ms(100);
    }
    
    // Cleanup
    FURI_LOG_I(TAG, "Shutting down...");
    
    furi_thread_free(rf_capture_thread);
    furi_thread_free(ui_update_thread);
    furi_thread_free(analysis_thread);
    
    view_dispatcher_free(view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    sd_manager_deinit();
    cc1101_driver_deinit();
    
    FURI_LOG_I(TAG, "Shutdown complete");
    
    return 0;
}
