#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <furi.h>
#include <furi_hal_gpio.h>
#include "../flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GPIO PIN MAPPINGS FOR FLIPPER ZERO
// ============================================================================

// External header pins (2.54mm headers)
#define GPIO_PIN_2      &gpio_ext_pa7   // PA7
#define GPIO_PIN_3      &gpio_ext_pa6   // PA6
#define GPIO_PIN_4      &gpio_ext_pa4   // PA4
#define GPIO_PIN_5      &gpio_ext_pb3   // PB3
#define GPIO_PIN_6      &gpio_ext_pb2   // PB2
#define GPIO_PIN_7      &gpio_ext_pc3   // PC3
#define GPIO_PIN_13     &gpio_usart_tx  // UART TX
#define GPIO_PIN_14     &gpio_usart_rx  // UART RX
#define GPIO_PIN_15     &gpio_ext_pc1   // PC1
#define GPIO_PIN_16     &gpio_ext_pc0   // PC0

// Internal pins
#define GPIO_SWDIO      &gpio_swd_io
#define GPIO_SWCLK      &gpio_swd_clk

// CC1101 connections (using external SPI)
#define GPIO_CC1101_CS      GPIO_PIN_4   // PA4
#define GPIO_CC1101_GDO0    GPIO_PIN_3   // PA6
#define GPIO_CC1101_GDO2    GPIO_PIN_2   // PA7

// ============================================================================
// GPIO CONFIGURATION STRUCTURES
// ============================================================================

typedef enum {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT_PP,    // Push-pull output
    GPIO_MODE_OUTPUT_OD,    // Open-drain output
    GPIO_MODE_AF_PP,        // Alternate function push-pull
    GPIO_MODE_AF_OD,        // Alternate function open-drain
    GPIO_MODE_ANALOG
} GPIOMode_t;

typedef enum {
    GPIO_SPEED_LOW,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERY_HIGH
} GPIOSpeed_t;

typedef enum {
    GPIO_PULL_NONE,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
} GPIOPull_t;

typedef struct {
    const GpioPin* pin;
    GPIOMode_t mode;
    GPIOSpeed_t speed;
    GPIOPull_t pull;
    bool initial_state;
    const char* name;
} GPIOConfig_t;

typedef struct {
    const GpioPin* pin;
    bool current_state;
    uint32_t last_change_time;
    uint32_t debounce_time_ms;
    bool debounced_state;
    void (*callback)(void* context);
    void* callback_context;
} GPIOInputState_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus gpio_manager_init(void);
void gpio_manager_deinit(void);

// Pin configuration
void gpio_configure_pin(const GpioPin* pin, GPIOMode_t mode, GPIOSpeed_t speed, GPIOPull_t pull);
void gpio_configure_output(const GpioPin* pin, bool initial_state);
void gpio_configure_input(const GpioPin* pin, GPIOPull_t pull);
void gpio_configure_interrupt(const GpioPin* pin, void (*callback)(void*), void* context);

// Pin operations
void gpio_write(const GpioPin* pin, bool state);
bool gpio_read(const GpioPin* pin);
void gpio_toggle(const GpioPin* pin);

// Debounced input reading
bool gpio_read_debounced(const GpioPin* pin, uint32_t debounce_ms);
void gpio_update_debounce(void);

// Interrupt management
void gpio_enable_interrupt(const GpioPin* pin);
void gpio_disable_interrupt(const GpioPin* pin);
void gpio_clear_interrupt(const GpioPin* pin);

// Batch operations
void gpio_write_multiple(const GpioPin** pins, bool* states, uint8_t count);
void gpio_read_multiple(const GpioPin** pins, bool* states, uint8_t count);

// Power management
void gpio_set_low_power_mode(bool enable);
void gpio_disable_unused_pins(void);

// Debug and diagnostics
void gpio_print_configuration(void);
uint32_t gpio_get_pin_state_mask(void);
bool gpio_verify_configuration(void);

// ============================================================================
// SPECIALIZED FUNCTIONS FOR RF RESEARCH
// ============================================================================

// Trigger input for external sensors
void gpio_configure_trigger_input(const GpioPin* pin, uint32_t trigger_edge);
bool gpio_check_trigger(const GpioPin* pin);

// Synchronous sampling for timing analysis
void gpio_start_synchronized_sampling(uint32_t sample_rate_hz);
void gpio_stop_synchronized_sampling(void);
bool gpio_get_sample(uint32_t* timestamp, bool* state);

// Pulse width measurement
uint32_t gpio_measure_pulse_width(const GpioPin* pin, bool target_level, uint32_t timeout_us);
uint32_t gpio_measure_interval(const GpioPin* pin, uint32_t* last_timestamp);

// Pattern detection
bool gpio_wait_for_pattern(const GpioPin* pin, const bool* pattern, uint8_t pattern_len, 
                           uint32_t bit_time_us, uint32_t timeout_ms);

// ============================================================================
// PIN GROUP MANAGEMENT
// ============================================================================

typedef struct {
    const GpioPin** pins;
    uint8_t count;
    uint32_t state_mask;
    char name[16];
} GPIOGroup_t;

void gpio_group_init(GPIOGroup_t* group, const char* name);
void gpio_group_add_pin(GPIOGroup_t* group, const GpioPin* pin);
void gpio_group_write(GPIOGroup_t* group, uint32_t value);
uint32_t gpio_group_read(GPIOGroup_t* group);

#ifdef __cplusplus
}
#endif

#endif // GPIO_MANAGER_H
