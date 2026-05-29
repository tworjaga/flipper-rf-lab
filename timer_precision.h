#include "cc1101_driver.h"
#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_interrupt.h>

#define TAG "CC1101"

#define CC1101_SPI_HANDLE   &furi_hal_spi_bus_handle_external
#define CC1101_CS_PIN       &gpio_ext_pa4
#define CC1101_GDO0_PIN     &gpio_ext_pa6
#define CC1101_GDO2_PIN     &gpio_ext_pa7

#define CC1101_SPI_TIMEOUT       1000
#define CC1101_RESET_DELAY_US    100
#define CC1101_CALIBRATE_TIME_US 750
// Max poll iterations for state-machine waits (each iteration ≈ 10 µs)
#define CC1101_STATE_WAIT_ITERS  1000

// ============================================================================
// STATIC STATE
// ============================================================================

static volatile bool    cc1101_initialized   = false;
static volatile bool    low_power_mode_flag  = false;
static CC1101Config_t   current_config;
static FuriMutex*       spi_mutex            = NULL;
static volatile uint32_t isr_count           = 0;
static volatile uint8_t  last_rssi           = 0;

// FIX (critical): ISR only sets a flag — no SPI, no mutex.
// FIX (critical): Use atomic exchange to avoid missed-IRQ race on read-clear.
static volatile bool rssi_sample_pending = false;

// Frequency-hopping state
static volatile bool     freq_hopping_enabled = false;
static volatile uint16_t hop_interval_ms      = 100;
static volatile uint32_t current_freq_index   = 0;
static uint32_t          hop_frequencies[16];
static uint8_t           num_hop_freqs        = 0;

// ============================================================================
// PRESET REGISTER TABLES  (32 bytes each, registers 0x00–0x1F)
// ============================================================================

const uint8_t CC1101_CONFIG_433_OOK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x10, 0xB0, 0x71,
    0x93, 0x83, 0x12, 0x15, 0x1C, 0x91, 0x09, 0x16,
    0x16, 0x17, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t CC1101_CONFIG_868_FSK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x21, 0x62, 0xF5,
    0x83, 0x13, 0x22, 0xF8, 0x15, 0x07, 0x30, 0x18,
    0x16, 0x6C, 0x03, 0x40, 0x91, 0x87, 0x6B, 0xFB
};

const uint8_t CC1101_CONFIG_915_GFSK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x23, 0x31, 0x3B,
    0xF8, 0x93, 0x03, 0x22, 0xF8, 0x15, 0x07, 0x30,
    0x18, 0x14, 0x6C, 0x07, 0x00, 0x91, 0x87, 0x6B
};

const uint8_t CC1101_CONFIG_315_ASK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x0C, 0x1D, 0x2C,
    0x93, 0x83, 0x12, 0x15, 0x1C, 0x91, 0x09, 0x16,
    0x16, 0x17, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ============================================================================
// INTERNAL (NO-LOCK) HELPERS — only call while spi_mutex + SPI bus are held
// ============================================================================

static void _spi_tx(const uint8_t* data, uint32_t len) {
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, data, len, CC1101_SPI_TIMEOUT);
}

static void _spi_rx(uint8_t* data, uint32_t len) {
    furi_hal_spi_bus_rx(CC1101_SPI_HANDLE, data, len, CC1101_SPI_TIMEOUT);
}

// Write a single register without acquiring lock (caller must hold both).
static void _write_reg_nolock(uint8_t reg, uint8_t value) {
    uint8_t addr = reg & 0x3F;
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&addr, 1);
    _spi_tx(&value, 1);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
}

// Read a single register without acquiring lock.
static uint8_t _read_reg_nolock(uint8_t reg) {
    uint8_t addr  = (reg & 0x3F) | CC1101_READ_SINGLE;
    uint8_t value = 0;
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&addr, 1);
    _spi_rx(&value, 1);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    return value;
}

// Write burst without acquiring lock.
static void _write_burst_nolock(uint8_t reg, const uint8_t* data, uint8_t len) {
    if(len == 0) return;
    uint8_t addr = (reg & 0x3F) | CC1101_WRITE_BURST;
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&addr, 1);
    _spi_tx(data, len);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
}

// Send a command strobe without acquiring lock.
static void _send_command_nolock(uint8_t cmd) {
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&cmd, 1);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
}

// Poll MARCSTATE until the chip reaches target_state or timeout.
// FIX (minor): calibrate() used a fixed delay; now polls for actual IDLE.
static bool _wait_for_state_nolock(uint8_t target_state, uint32_t max_iters) {
    for(uint32_t i = 0; i < max_iters; i++) {
        uint8_t state = _read_reg_nolock(CC1101_MARCSTATE) & 0x1F;
        if(state == target_state) return true;
        furi_delay_us(10);
    }
    return false;
}

// ============================================================================
// ISR
// ============================================================================

static void cc1101_gdo0_isr(void* context) {
    UNUSED(context);
    isr_count++;
    // FIX (critical): flag-only ISR — no SPI, no mutex, no function calls.
    rssi_sample_pending = true;
}

// ============================================================================
// PUBLIC API — THREAD-SAFE LOCKED FUNCTIONS
// ============================================================================

// FIX (critical): Service RSSI from thread context using cc1101_read_rssi()
//                 which holds the mutex, not the raw bypass function.
// FIX (critical): Atomic test-and-clear to avoid lost-IRQ race.
void cc1101_service_rssi_pending(void) {
    bool was_pending = __atomic_exchange_n(
        (bool*)&rssi_sample_pending, false, __ATOMIC_SEQ_CST);
    if(was_pending) {
        // cc1101_read_rssi() acquires mutex and SPI bus properly.
        last_rssi = cc1101_read_rssi();
    }
}

FuriStatus cc1101_driver_init(void) {
    FURI_LOG_I(TAG, "Initializing CC1101 driver");

    if(cc1101_initialized) {
        FURI_LOG_W(TAG, "CC1101 already initialized");
        return FuriStatusOk;
    }

    spi_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!spi_mutex) {
        FURI_LOG_E(TAG, "Failed to allocate SPI mutex");
        return FuriStatusError;
    }

    furi_hal_gpio_init(CC1101_CS_PIN,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_hal_gpio_init(CC1101_GDO0_PIN, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(CC1101_GDO2_PIN, GpioModeInput, GpioPullNo, GpioSpeedLow);

    // FIX (critical): Acquire mutex + bus ONCE and use nolock helpers for all
    //                 init-time register access.  Previous code re-entered the
    //                 mutex (non-recursive) through reset/load/calibrate, which
    //                 deadlocked on the second furi_mutex_acquire call.
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);

    // Reset
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(10);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(40);
    _send_command_nolock(CC1101_SRES);
    furi_delay_us(CC1101_RESET_DELAY_US);

    // Verify chip
    uint8_t partnum = _read_reg_nolock(CC1101_PARTNUM);
    uint8_t version = _read_reg_nolock(CC1101_VERSION);
    FURI_LOG_I(TAG, "CC1101 PartNum: 0x%02X, Version: 0x%02X", partnum, version);

    if(partnum != 0x00) {
        FURI_LOG_E(TAG, "CC1101 not detected (wrong part number)");
        furi_hal_spi_release(CC1101_SPI_HANDLE);
        furi_mutex_release(spi_mutex);
        furi_mutex_free(spi_mutex);
        spi_mutex = NULL;
        return FuriStatusError;
    }

    // Load default config
    _write_burst_nolock(0x00, CC1101_CONFIG_433_OOK, 32);

    // Calibrate — poll for IDLE instead of fixed delay
    _send_command_nolock(CC1101_SCAL);
    if(!_wait_for_state_nolock(CC1101_STATE_IDLE, CC1101_STATE_WAIT_ITERS)) {
        FURI_LOG_W(TAG, "Calibration timeout");
    }
    FURI_LOG_I(TAG, "Frequency synthesizer calibrated");

    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);

    // FIX (major): Enable interrupt AFTER releasing the bus so the ISR cannot
    //              fire while the bus is still held during init.
    furi_hal_gpio_add_int_callback(CC1101_GDO0_PIN, cc1101_gdo0_isr, NULL);

    cc1101_initialized = true;
    FURI_LOG_I(TAG, "CC1101 driver initialized successfully");
    return FuriStatusOk;
}

void cc1101_driver_deinit(void) {
    if(!cc1101_initialized) return;
    FURI_LOG_I(TAG, "Deinitializing CC1101 driver");
    furi_hal_gpio_remove_int_callback(CC1101_GDO0_PIN);
    cc1101_enter_idle();
    furi_mutex_free(spi_mutex);
    spi_mutex = NULL;
    cc1101_initialized = false;
}

// ============================================================================
// LOCKED PUBLIC REGISTER ACCESS
// ============================================================================

void cc1101_write_register(uint8_t reg, uint8_t value) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _write_reg_nolock(reg, value);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

uint8_t cc1101_read_register(uint8_t reg) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t value = _read_reg_nolock(reg);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    return value;
}

void cc1101_write_burst(uint8_t reg, const uint8_t* data, uint8_t len) {
    if(len == 0) return;
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _write_burst_nolock(reg, data, len);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_read_burst(uint8_t reg, uint8_t* data, uint8_t len) {
    if(len == 0) return;
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t addr = (reg & 0x3F) | CC1101_READ_BURST;
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&addr, 1);
    _spi_rx(data, len);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_send_command(uint8_t cmd) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _send_command_nolock(cmd);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

// ============================================================================
// STATUS / CONTROL
// ============================================================================

CC1101Status_t cc1101_get_status(void) {
    CC1101Status_t status;
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    status.partnum   = _read_reg_nolock(CC1101_PARTNUM);
    status.version   = _read_reg_nolock(CC1101_VERSION);
    status.rssi      = _read_reg_nolock(CC1101_RSSI);
    status.lqi       = _read_reg_nolock(CC1101_LQI);
    status.marcstate = _read_reg_nolock(CC1101_MARCSTATE);
    status.pktstatus = _read_reg_nolock(CC1101_PKTSTATUS);
    status.rxbytes   = _read_reg_nolock(CC1101_RXBYTES) & 0x7F;
    status.txbytes   = _read_reg_nolock(CC1101_TXBYTES) & 0x7F;
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    return status;
}

uint8_t cc1101_get_state(void) {
    return cc1101_read_register(CC1101_MARCSTATE) & 0x1F;
}

bool cc1101_has_data(void) {
    uint8_t rxbytes = cc1101_read_register(CC1101_RXBYTES);
    return ((rxbytes & 0x7F) > 0) && !(rxbytes & 0x80);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void cc1101_set_frequency(uint32_t freq_hz) {
    uint32_t freq_word = ((uint64_t)freq_hz * 65536ULL) / 26000000ULL;
    uint8_t freq2 = (freq_word >> 16) & 0xFF;
    uint8_t freq1 = (freq_word >>  8) & 0xFF;
    uint8_t freq0 =  freq_word        & 0xFF;

    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _write_reg_nolock(CC1101_FREQ2, freq2);
    _write_reg_nolock(CC1101_FREQ1, freq1);
    _write_reg_nolock(CC1101_FREQ0, freq0);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);

    current_config.frequency_hz = freq_hz;
    FURI_LOG_D(TAG, "Frequency set to %lu Hz (word: 0x%02X%02X%02X)",
               freq_hz, freq2, freq1, freq0);
}

void cc1101_set_data_rate(uint32_t baud) {
    uint32_t drate   = ((uint64_t)baud << 28) / 26000000ULL;
    uint8_t  drate_e = 0;
    uint32_t drate_m = drate;
    while(drate_m > 255 && drate_e < 15) { drate_m >>= 1; drate_e++; }

    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t mdmcfg4 = _read_reg_nolock(CC1101_MDMCFG4);
    mdmcfg4 = (mdmcfg4 & 0xF0) | drate_e;
    _write_reg_nolock(CC1101_MDMCFG4, mdmcfg4);
    _write_reg_nolock(CC1101_MDMCFG3, (uint8_t)drate_m);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);

    current_config.data_rate = baud;
    FURI_LOG_D(TAG, "Data rate set to %lu baud (E=%d, M=%d)", baud, drate_e, (uint8_t)drate_m);
}

void cc1101_set_modulation(uint8_t modulation) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t mdmcfg2 = _read_reg_nolock(CC1101_MDMCFG2) & 0x8F;
    switch(modulation) {
        case MOD_2FSK: mdmcfg2 |= 0x00; break;
        case MOD_GFSK: mdmcfg2 |= 0x10; break;
        case MOD_ASK:
        case MOD_OOK:  mdmcfg2 |= 0x30; break;
        case MOD_4FSK: mdmcfg2 |= 0x40; break;
        case MOD_MSK:  mdmcfg2 |= 0x70; break;
        default:
            FURI_LOG_W(TAG, "Unknown modulation type: %d", modulation);
            furi_hal_spi_release(CC1101_SPI_HANDLE);
            furi_mutex_release(spi_mutex);
            return;
    }
    _write_reg_nolock(CC1101_MDMCFG2, mdmcfg2);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    current_config.modulation = modulation;
    FURI_LOG_D(TAG, "Modulation set to %d", modulation);
}

// FIX (critical): parameter changed from uint8_t to int8_t so negative dBm
//                 values are not silently wrapped before the comparisons.
void cc1101_set_tx_power(int8_t power_dbm) {
    uint8_t pa_table[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // FIX (minor): extended range to +12 dBm per datasheet PA table
    if     (power_dbm >= 12) pa_table[0] = 0xC2;
    else if(power_dbm >= 10) pa_table[0] = 0xC0;
    else if(power_dbm >=  7) pa_table[0] = 0xC8;
    else if(power_dbm >=  5) pa_table[0] = 0x84;
    else if(power_dbm >=  0) pa_table[0] = 0x60;
    else if(power_dbm >= -6) pa_table[0] = 0x50;
    else if(power_dbm >=-10) pa_table[0] = 0x34;
    else if(power_dbm >=-15) pa_table[0] = 0x1D;
    else                     pa_table[0] = 0x12;   // -20 dBm

    cc1101_write_burst(CC1101_PATABLE, pa_table, 8);
    current_config.tx_power = power_dbm;
    FURI_LOG_D(TAG, "TX power set to %d dBm", power_dbm);
}

void cc1101_set_channel(uint8_t channel) {
    cc1101_write_register(CC1101_CHANNR, channel);
}

// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void cc1101_enter_rx(void) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _send_command_nolock(CC1101_SRX);
    _wait_for_state_nolock(CC1101_STATE_RX, CC1101_STATE_WAIT_ITERS);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_enter_tx(void) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _send_command_nolock(CC1101_STX);
    _wait_for_state_nolock(CC1101_STATE_TX, CC1101_STATE_WAIT_ITERS);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_enter_idle(void) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _send_command_nolock(CC1101_SIDLE);
    _wait_for_state_nolock(CC1101_STATE_IDLE, CC1101_STATE_WAIT_ITERS);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_flush_rx(void) { cc1101_send_command(CC1101_SFRX); }
void cc1101_flush_tx(void) { cc1101_send_command(CC1101_SFTX); }

// ============================================================================
// DATA
// ============================================================================

uint8_t cc1101_read_rssi(void) {
    return cc1101_read_register(CC1101_RSSI);
}

// FIX (critical): cc1101_read_rssi_live() removed entirely.
//                 It bypassed the SPI mutex and was therefore unsafe from any
//                 thread context. cc1101_service_rssi_pending() now calls
//                 cc1101_read_rssi() which properly locks.

#define CC1101_RSSI_OFFSET 74

int16_t cc1101_rssi_to_dbm(uint8_t rssi_reg) {
    if(rssi_reg >= 128)
        return (int16_t)((int16_t)rssi_reg - 256) / 2 - CC1101_RSSI_OFFSET;
    return (int16_t)rssi_reg / 2 - CC1101_RSSI_OFFSET;
}

bool cc1101_receive_packet(uint8_t* data, uint8_t* len, uint8_t* rssi, uint8_t* lqi) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);

    uint8_t rxbytes = _read_reg_nolock(CC1101_RXBYTES);
    if(rxbytes & 0x80) {
        FURI_LOG_W(TAG, "RX FIFO overflow");
        _send_command_nolock(CC1101_SFRX);
        furi_hal_spi_release(CC1101_SPI_HANDLE);
        furi_mutex_release(spi_mutex);
        return false;
    }

    uint8_t pktlen = rxbytes & 0x7F;
    if(pktlen == 0) {
        furi_hal_spi_release(CC1101_SPI_HANDLE);
        furi_mutex_release(spi_mutex);
        return false;
    }

    uint8_t fifo_data[64];
    uint8_t read_len = (pktlen > 64) ? 64 : pktlen;
    uint8_t addr = (CC1101_RXFIFO & 0x3F) | CC1101_READ_BURST;
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    _spi_tx(&addr, 1);
    _spi_rx(fifo_data, read_len);
    furi_hal_gpio_write(CC1101_CS_PIN, true);

    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);

    uint8_t actual_len = fifo_data[0];
    if(actual_len > 60) actual_len = 60;
    memcpy(data, &fifo_data[1], actual_len);
    *len = actual_len;
    if(rssi && pktlen >= 2) *rssi = fifo_data[pktlen - 2];
    if(lqi  && pktlen >= 1) *lqi  = fifo_data[pktlen - 1];
    return true;
}

bool cc1101_transmit_packet(const uint8_t* data, uint8_t len) {
    if(len > 60) {
        FURI_LOG_E(TAG, "Packet too long: %d bytes", len);
        return false;
    }

    uint8_t tx_data[65];
    tx_data[0] = len;
    memcpy(&tx_data[1], data, len);
    cc1101_flush_tx();
    cc1101_write_burst(CC1101_TXFIFO, tx_data, len + 1);
    cc1101_enter_tx();

    uint32_t timeout = 10000;
    while(cc1101_get_state() == CC1101_STATE_TX && --timeout > 0) {
        furi_delay_us(10);
    }
    if(timeout == 0) {
        FURI_LOG_E(TAG, "TX timeout");
        cc1101_enter_idle();
        return false;
    }
    return true;
}

// ============================================================================
// ADVANCED
// ============================================================================

void cc1101_set_low_power_mode(bool enable) {
    low_power_mode_flag = enable;
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t mcsm2 = _read_reg_nolock(CC1101_MCSM2);
    if(enable)
        mcsm2 |=  0x07;
    else
        mcsm2 &= ~0x07;
    _write_reg_nolock(CC1101_MCSM2, mcsm2);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    FURI_LOG_I(TAG, "Low power mode %s", enable ? "enabled" : "disabled");
}

// FIX (minor): Poll MARCSTATE == IDLE instead of fixed 750 µs delay.
void cc1101_calibrate(void) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _send_command_nolock(CC1101_SCAL);
    if(!_wait_for_state_nolock(CC1101_STATE_IDLE, CC1101_STATE_WAIT_ITERS)) {
        FURI_LOG_W(TAG, "Calibration wait timeout");
    }
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    FURI_LOG_I(TAG, "Frequency synthesizer calibrated");
}

void cc1101_set_sync_word(const uint8_t* sync_word) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    _write_reg_nolock(CC1101_SYNC1, sync_word[0]);
    _write_reg_nolock(CC1101_SYNC0, sync_word[1]);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    current_config.sync_word[0] = sync_word[0];
    current_config.sync_word[1] = sync_word[1];
}

void cc1101_set_preamble(uint8_t preamble_bytes) {
    uint8_t preamble_cfg = 0;
    if     (preamble_bytes >= 24) preamble_cfg = 7;
    else if(preamble_bytes >= 16) preamble_cfg = 6;
    else if(preamble_bytes >= 12) preamble_cfg = 5;
    else if(preamble_bytes >=  8) preamble_cfg = 4;
    else if(preamble_bytes >=  6) preamble_cfg = 3;
    else if(preamble_bytes >=  4) preamble_cfg = 2;
    else if(preamble_bytes >=  3) preamble_cfg = 1;

    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    uint8_t mdmcfg1 = (_read_reg_nolock(CC1101_MDMCFG1) & 0x8F) | (preamble_cfg << 4);
    _write_reg_nolock(CC1101_MDMCFG1, mdmcfg1);
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

void cc1101_load_preset_config(const uint8_t* config) {
    cc1101_write_burst(0x00, config, 32);
    FURI_LOG_I(TAG, "Preset configuration loaded");
}

// ============================================================================
// RSSI SAMPLING (stub — requires GDO0 hardware configuration)
// ============================================================================

void cc1101_start_rssi_sampling(uint16_t sample_rate_hz) {
    UNUSED(sample_rate_hz);
    // TODO: configure GDO0 to signal RSSI threshold crossings and enable
    //       continuous sampling at the requested rate via a timer callback.
    FURI_LOG_W(TAG, "cc1101_start_rssi_sampling: NOT IMPLEMENTED");
}

void cc1101_stop_rssi_sampling(void) {
    FURI_LOG_W(TAG, "cc1101_stop_rssi_sampling: NOT IMPLEMENTED");
}

uint8_t cc1101_get_rssi_sample(void) {
    return last_rssi;
}

// ============================================================================
// FREQUENCY HOPPING
// ============================================================================

void cc1101_set_frequency_hopping(bool enable, uint16_t interval_ms) {
    freq_hopping_enabled = enable;
    hop_interval_ms      = interval_ms;
    FURI_LOG_I(TAG, "Frequency hopping %s, interval: %d ms",
               enable ? "enabled" : "disabled", interval_ms);
}

void cc1101_set_hop_frequencies(const uint32_t* freqs, uint8_t count) {
    if(count > 16) count = 16;
    memcpy(hop_frequencies, freqs, count * sizeof(uint32_t));
    num_hop_freqs       = count;
    current_freq_index  = 0;
}

void cc1101_hop_frequency(void) {
    if(!freq_hopping_enabled || num_hop_freqs == 0) return;
    current_freq_index = (current_freq_index + 1) % num_hop_freqs;
    cc1101_set_frequency(hop_frequencies[current_freq_index]);
}

void cc1101_reset(void) {
    // Callers outside init should hold the mutex before calling this.
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(10);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(40);
    _send_command_nolock(CC1101_SRES);
    furi_delay_us(CC1101_RESET_DELAY_US);
}
