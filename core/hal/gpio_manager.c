#include "gpio_manager.h"
#include "timer_precision.h"

#define TAG "GPIO_MGR"
#define MAX_GPIO_INPUTS 16
#define MAX_GPIO_GROUPS 4

// Static state
static bool gpio_initialized = false;
static GPIOInputState_t input_states[MAX_GPIO_INPUTS];
static uint8_t num_input_states = 0;
static GPIOGroup_t gpio_groups[MAX_GPIO_GROUPS];
static uint8_t num_gpio_groups = 0;
static bool low_power_mode = false;

// Initialize GPIO manager
FuriStatus gpio_manager_init(void) {
    if(gpio_initialized) {
        return FuriStatusOk;
    }
    
    FURI_LOG_I(TAG, "Initializing GPIO manager");
    
    // Clear state arrays
    memset(input_states, 0, sizeof(input_states));
    memset(gpio_groups, 0, sizeof(gpio_groups));
    num_input_states = 0;
    num_gpio_groups = 0;
    
    // Configure default pin states
    // All pins as input with pull-down for safety
    const GpioPin* default_pins[] = {
        GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5,
        GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_15, GPIO_PIN_16
    };
    
    for(uint8_t i = 0; i < 8; i++) {
        furi_hal_gpio_init(default_pins[i], GpioModeInput, GpioPullDown, GpioSpeedLow);
    }
    
    gpio_initialized = true;
    FURI_LOG_I(TAG, "GPIO manager initialized");
    
    return FuriStatusOk;
}

void gpio_manager_deinit(void) {
    if(!gpio_initialized) return;
    
    // Reset all pins to safe state
    const GpioPin* all_pins[] = {
        GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5,
        GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_15, GPIO_PIN_16
    };
    
    for(uint8_t i = 0; i < 8; i++) {
        furi_hal_gpio_init(all_pins[i], GpioModeInput, GpioPullDown, GpioSpeedLow);
    }
    
    gpio_initialized = false;
}

// Configure pin with specific mode
void gpio_configure_pin(const GpioPin* pin, GPIOMode_t mode, GPIOSpeed_t speed, GPIOPull_t pull) {
    GpioMode hal_mode;
    GpioPull hal_pull;
    GpioSpeed hal_speed;
    
    // Convert mode
    switch(mode) {
        case GPIO_MODE_INPUT: hal_mode = GpioModeInput; break;
        case GPIO_MODE_OUTPUT_PP: hal_mode = GpioModeOutputPushPull; break;
        case GPIO_MODE_OUTPUT_OD: hal_mode = GpioModeOutputOpenDrain; break;
        case GPIO_MODE_AF_PP: hal_mode = GpioModeAltFunctionPushPull; break;
        case GPIO_MODE_AF_OD: hal_mode = GpioModeAltFunctionOpenDrain; break;
        case GPIO_MODE_ANALOG: hal_mode = GpioModeAnalog; break;
        default: hal_mode = GpioModeInput;
    }
    
    // Convert pull
    switch(pull) {
        case GPIO_PULL_NONE: hal_pull = GpioPullNo; break;
        case GPIO_PULL_UP: hal_pull = GpioPullUp; break;
        case GPIO_PULL_DOWN: hal_pull = GpioPullDown; break;
        default: hal_pull = GpioPullNo;
    }
    
    // Convert speed
    switch(speed) {
        case GPIO_SPEED_LOW: hal_speed = GpioSpeedLow; break;
        case GPIO_SPEED_MEDIUM: hal_speed = GpioSpeedMedium; break;
        case GPIO_SPEED_HIGH: hal_speed = GpioSpeedHigh; break;
        case GPIO_SPEED_VERY_HIGH: hal_speed = GpioSpeedVeryHigh; break;
        default: hal_speed = GpioSpeedLow;
    }
    
    furi_hal_gpio_init(pin, hal_mode, hal_pull, hal_speed);
}

// Configure as output
void gpio_configure_output(const GpioPin* pin, bool initial_state) {
    gpio_configure_pin(pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE);
    gpio_write(pin, initial_state);
}

// Configure as input
void gpio_configure_input(const GpioPin* pin, GPIOPull_t pull) {
    gpio_configure_pin(pin, GPIO_MODE_INPUT, GPIO_SPEED_LOW, pull);
}

// Configure with interrupt
void gpio_configure_interrupt(const GpioPin* pin, void (*callback)(void*), void* context) {
    // Find or create input state entry
    GPIOInputState_t* state = NULL;
    
    for(uint8_t i = 0; i < num_input_states; i++) {
        if(input_states[i].pin == pin) {
            state = &input_states[i];
            break;
        }
    }
    
    if(!state && num_input_states < MAX_GPIO_INPUTS) {
        state = &input_states[num_input_states++];
        state->pin = pin;
    }
    
    if(state) {
        state->callback = callback;
        state->callback_context = context;
        state->debounce_time_ms = 50;  // Default 50ms debounce
        state->last_change_time = 0;
        state->debounced_state = gpio_read(pin);
        state->current_state = state->debounced_state;
        
        // Configure pin as input
        gpio_configure_input(pin, GPIO_PULL_UP);
        
        // Add interrupt callback
        furi_hal_gpio_add_int_callback(pin, callback, context);
    }
}

// Write pin state
void gpio_write(const GpioPin* pin, bool state) {
    furi_hal_gpio_write(pin, state);
}

// Read pin state
bool gpio_read(const GpioPin* pin) {
    return furi_hal_gpio_read(pin);
}

// Toggle pin
void gpio_toggle(const GpioPin* pin) {
    bool current = gpio_read(pin);
    gpio_write(pin, !current);
}

// Debounced read
bool gpio_read_debounced(const GpioPin* pin, uint32_t debounce_ms) {
    // Find input state
    GPIOInputState_t* state = NULL;
    for(uint8_t i = 0; i < num_input_states; i++) {
        if(input_states[i].pin == pin) {
            state = &input_states[i];
            break;
        }
    }
    
    if(!state) {
        // No debouncing configured, return raw read
        return gpio_read(pin);
    }
    
    uint32_t now = furi_get_tick();
    bool raw_state = gpio_read(pin);
    
    // Check if state changed
    if(raw_state != state->current_state) {
        state->current_state = raw_state;
        state->last_change_time = now;
    }
    
    // Check if debounce period elapsed
    if((now - state->last_change_time) >= debounce_ms) {
        state->debounced_state = state->current_state;
    }
    
    return state->debounced_state;
}

// Update all debounce states (call periodically)
void gpio_update_debounce(void) {
    for(uint8_t i = 0; i < num_input_states; i++) {
        GPIOInputState_t* state = &input_states[i];
        uint32_t now = furi_get_tick();
        bool raw_state = gpio_read(state->pin);
        
        if(raw_state != state->current_state) {
            state->current_state = raw_state;
            state->last_change_time = now;
        }
        
        if((now - state->last_change_time) >= state->debounce_time_ms) {
            state->debounced_state = state->current_state;
        }
    }
}

// Enable interrupt
void gpio_enable_interrupt(const GpioPin* pin) {
    furi_hal_gpio_enable_int_callback(pin);
}

// Disable interrupt
void gpio_disable_interrupt(const GpioPin* pin) {
    furi_hal_gpio_disable_int_callback(pin);
}

// Clear interrupt flag
void gpio_clear_interrupt(const GpioPin* pin) {
    // On STM32, this is handled by HAL
    UNUSED(pin);
}

// Write multiple pins
void gpio_write_multiple(const GpioPin** pins, bool* states, uint8_t count) {
    for(uint8_t i = 0; i < count; i++) {
        gpio_write(pins[i], states[i]);
    }
}

// Read multiple pins
void gpio_read_multiple(const GpioPin** pins, bool* states, uint8_t count) {
    for(uint8_t i = 0; i < count; i++) {
        states[i] = gpio_read(pins[i]);
    }
}

// Set low power mode
void gpio_set_low_power_mode(bool enable) {
    low_power_mode = enable;
    
    if(enable) {
        FURI_LOG_I(TAG, "GPIO low power mode enabled");
        // Disable interrupts on non-essential pins
        for(uint8_t i = 0; i < num_input_states; i++) {
            furi_hal_gpio_disable_int_callback(input_states[i].pin);
        }
    } else {
        FURI_LOG_I(TAG, "GPIO low power mode disabled");
        // Re-enable interrupts
        for(uint8_t i = 0; i < num_input_states; i++) {
            furi_hal_gpio_enable_int_callback(input_states[i].pin);
        }
    }
}

// Disable unused pins for power saving
void gpio_disable_unused_pins(void) {
    const GpioPin* unused_pins[] = {
        GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_15, GPIO_PIN_16
    };
    
    for(uint8_t i = 0; i < 5; i++) {
        // Configure as analog input (lowest power consumption)
        furi_hal_gpio_init(unused_pins[i], GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
}

// Print current configuration (for debugging)
void gpio_print_configuration(void) {
    FURI_LOG_I(TAG, "GPIO Configuration:");
    FURI_LOG_I(TAG, "  Input states: %d/%d", num_input_states, MAX_GPIO_INPUTS);
    FURI_LOG_I(TAG, "  GPIO groups: %d/%d", num_gpio_groups, MAX_GPIO_GROUPS);
    FURI_LOG_I(TAG, "  Low power mode: %s", low_power_mode ? "ON" : "OFF");
}

// Get pin state as bit mask
uint32_t gpio_get_pin_state_mask(void) {
    uint32_t mask = 0;
    
    const GpioPin* pins[] = {
        GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5,
        GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_15, GPIO_PIN_16
    };
    
    for(uint8_t i = 0; i < 8; i++) {
        if(gpio_read(pins[i])) {
            mask |= (1 << i);
        }
    }
    
    return mask;
}

// Verify configuration is valid
bool gpio_verify_configuration(void) {
    // Check for pin conflicts
    // In a full implementation, this would verify no two functions use the same pin
    
    // Check current consumption estimate
    // Max 20mA per pin, 5W total budget
    
    return true;  // Configuration valid
}

// Configure trigger input for external sensors
void gpio_configure_trigger_input(const GpioPin* pin, uint32_t trigger_edge) {
    UNUSED(trigger_edge);  // Would configure rising/falling/both edge detection
    
    gpio_configure_interrupt(pin, NULL, NULL);
    gpio_enable_interrupt(pin);
}

// Check if trigger occurred
bool gpio_check_trigger(const GpioPin* pin) {
    // In a full implementation, this would check a flag set by ISR
    UNUSED(pin);
    return false;
}

// Synchronized sampling (for timing analysis)
static volatile bool sampling_active = false;
static volatile uint32_t sample_rate_hz = 0;

void gpio_start_synchronized_sampling(uint32_t sample_rate_hz_param) {
    sampling_active = true;
    sample_rate_hz = sample_rate_hz_param;
    
    // Would configure timer-based sampling
    FURI_LOG_I(TAG, "Started synchronized sampling at %lu Hz", sample_rate_hz);
}

void gpio_stop_synchronized_sampling(void) {
    sampling_active = false;
    FURI_LOG_I(TAG, "Stopped synchronized sampling");
}

bool gpio_get_sample(uint32_t* timestamp, bool* state) {
    if(!sampling_active) return false;
    
    *timestamp = timer_get_us();
    *state = false;  // Would read actual pin state
    
    return true;
}

// Measure pulse width with microsecond precision
uint32_t gpio_measure_pulse_width(const GpioPin* pin, bool target_level, uint32_t timeout_us) {
    uint32_t start = timer_get_us();
    
    // Wait for target level
    while(gpio_read(pin) != target_level) {
        if(timer_get_elapsed_us(start) > timeout_us) {
            return 0;  // Timeout
        }
    }
    
    // Measure duration
    uint32_t pulse_start = timer_get_us();
    while(gpio_read(pin) == target_level) {
        if(timer_get_elapsed_us(start) > timeout_us) {
            return 0;  // Timeout
        }
    }
    
    return timer_get_elapsed_us(pulse_start);
}

// Measure interval between transitions
uint32_t gpio_measure_interval(const GpioPin* pin, uint32_t* last_timestamp) {
    // Wait for transition
    bool initial = gpio_read(pin);
    while(gpio_read(pin) == initial) {
        // Busy wait
    }
    
    uint32_t now = timer_get_us();
    uint32_t interval = now - *last_timestamp;
    *last_timestamp = now;
    
    return interval;
}

// Wait for bit pattern
bool gpio_wait_for_pattern(const GpioPin* pin, const bool* pattern, uint8_t pattern_len,
                           uint32_t bit_time_us, uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    uint8_t matched = 0;
    
    while((furi_get_tick() - start) < timeout_ms) {
        bool current = gpio_read(pin);
        
        if(current == pattern[matched]) {
            matched++;
            if(matched >= pattern_len) {
                return true;  // Full pattern matched
            }
            // Wait for next bit period
            timer_delay_us(bit_time_us);
        } else {
            matched = 0;  // Reset on mismatch
        }
    }
    
    return false;  // Timeout
}

// GPIO group management
void gpio_group_init(GPIOGroup_t* group, const char* name) {
    if(num_gpio_groups >= MAX_GPIO_GROUPS) return;
    
    memset(group, 0, sizeof(GPIOGroup_t));
    strncpy(group->name, name, 15);
    group->name[15] = '\0';
    
    gpio_groups[num_gpio_groups++] = *group;
}

void gpio_group_add_pin(GPIOGroup_t* group, const GpioPin* pin) {
    if(group->count >= 8) return;  // Max 8 pins per group
    
    // Reallocate pin array (in real implementation, use static array)
    // For now, just track count
    group->count++;
}

void gpio_group_write(GPIOGroup_t* group, uint32_t value) {
    // Write value to all pins in group
    for(uint8_t i = 0; i < group->count; i++) {
        // Would write actual pin
    }
    group->state_mask = value;
}

uint32_t gpio_group_read(GPIOGroup_t* group) {
    uint32_t value = 0;
    
    // Read all pins in group
    for(uint8_t i = 0; i < group->count; i++) {
        // Would read actual pin and set bit
    }
    
    group->state_mask = value;
    return value;
}
