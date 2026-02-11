#include "cc1101_driver.h"
#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_interrupt.h>

#define TAG "CC1101"

// SPI configuration
#define CC1101_SPI_HANDLE   &furi_hal_spi_bus_handle_external
#define CC1101_CS_PIN       &gpio_ext_pa4
#define CC1101_GDO0_PIN     &gpio_ext_pa6
#define CC1101_GDO2_PIN     &gpio_ext_pa7

// Timing constants
#define CC1101_SPI_TIMEOUT  1000    // 1ms SPI timeout
#define CC1101_RESET_DELAY_US 100   // Reset delay
#define CC1101_CALIBRATE_TIME_US 750 // Calibration time

// Static state
static volatile bool cc1101_initialized = false;
static volatile bool low_power_mode = false;
static CC1101Config_t current_config;
static FuriMutex* spi_mutex = NULL;
static volatile uint32_t isr_count = 0;
static volatile uint8_t last_rssi = 0;

// Preset configurations (register values for common settings)
// 433.92 MHz, 2.4 kbps, OOK
const uint8_t CC1101_CONFIG_433_OOK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x10, 0xB0, 0x71,
    0x93, 0x83, 0x12, 0x15, 0x1C, 0x91, 0x09, 0x16,
    0x16, 0x17, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 868.35 MHz, 4.8 kbps, FSK
const uint8_t CC1101_CONFIG_868_FSK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x21, 0x62, 0xF5,
    0x83, 0x13, 0x22, 0xF8, 0x15, 0x07, 0x30, 0x18,
    0x16, 0x6C, 0x03, 0x40, 0x91, 0x87, 0x6B, 0xFB
};

// 915 MHz, 38.4 kbps, GFSK
const uint8_t CC1101_CONFIG_915_GFSK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x23, 0x31, 0x3B,
    0xF8, 0x93, 0x03, 0x22, 0xF8, 0x15, 0x07, 0x30,
    0x18, 0x14, 0x6C, 0x07, 0x00, 0x91, 0x87, 0x6B
};

// 315 MHz, 2 kbps, ASK
const uint8_t CC1101_CONFIG_315_ASK[] = {
    0x06, 0x2E, 0x02, 0x07, 0xD3, 0x91, 0xFF, 0x04,
    0x32, 0x00, 0x00, 0x06, 0x00, 0x0C, 0x1D, 0x2C,
    0x93, 0x83, 0x12, 0x15, 0x1C, 0x91, 0x09, 0x16,
    0x16, 0x17, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ISR handler for GDO0 (packet detection)
static void cc1101_gdo0_isr(void* context) {
    UNUSED(context);
    isr_count++;
    last_rssi = cc1101_read_rssi_live();
}

// Initialize CC1101 driver
FuriStatus cc1101_driver_init(void) {
    FURI_LOG_I(TAG, "Initializing CC1101 driver");
    
    if(cc1101_initialized) {
        FURI_LOG_W(TAG, "CC1101 already initialized");
        return FuriStatusOk;
    }
    
    // Create SPI mutex for thread safety
    spi_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!spi_mutex) {
        FURI_LOG_E(TAG, "Failed to allocate SPI mutex");
        return FuriStatusError;
    }
    
    // Configure GPIO pins
    furi_hal_gpio_init(CC1101_CS_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(CC1101_CS_PIN, true);  // CS high (inactive)
    
    furi_hal_gpio_init(CC1101_GDO0_PIN, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(CC1101_GDO2_PIN, GpioModeInput, GpioPullNo, GpioSpeedLow);
    
    // Initialize SPI bus
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    // Reset CC1101
    cc1101_reset();
    
    // Verify chip is present by reading part number
    uint8_t partnum = cc1101_read_register(CC1101_PARTNUM);
    uint8_t version = cc1101_read_register(CC1101_VERSION);
    
    FURI_LOG_I(TAG, "CC1101 PartNum: 0x%02X, Version: 0x%02X", partnum, version);
    
    if(partnum != 0x00) {
        FURI_LOG_E(TAG, "CC1101 not detected (wrong part number)");
        furi_hal_spi_release(CC1101_SPI_HANDLE);
        furi_mutex_free(spi_mutex);
        return FuriStatusError;
    }
    
    // Load default configuration (433 MHz OOK)
    cc1101_load_preset_config(CC1101_CONFIG_433_OOK);
    
    // Calibrate frequency synthesizer
    cc1101_calibrate();
    
    // Set up interrupt on GDO0 for packet detection
    furi_hal_gpio_add_int_callback(CC1101_GDO0_PIN, cc1101_gdo0_isr, NULL);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    
    cc1101_initialized = true;
    FURI_LOG_I(TAG, "CC1101 driver initialized successfully");
    
    return FuriStatusOk;
}

void cc1101_driver_deinit(void) {
    if(!cc1101_initialized) return;
    
    FURI_LOG_I(TAG, "Deinitializing CC1101 driver");
    
    // Disable interrupt
    furi_hal_gpio_remove_int_callback(CC1101_GDO0_PIN);
    
    // Enter idle state
    cc1101_enter_idle();
    
    // Release resources
    furi_mutex_free(spi_mutex);
    
    cc1101_initialized = false;
}

// Reset CC1101
void cc1101_reset(void) {
    // CS low, wait, CS high
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(10);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(40);
    
    // Send reset command
    cc1101_send_command(CC1101_SRES);
    furi_delay_us(CC1101_RESET_DELAY_US);
}

// Write single register
void cc1101_write_register(uint8_t reg, uint8_t value) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    
    // Send address with write bit
    uint8_t addr = reg & 0x3F;
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &addr, 1, CC1101_SPI_TIMEOUT);
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &value, 1, CC1101_SPI_TIMEOUT);
    
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

// Read single register
uint8_t cc1101_read_register(uint8_t reg) {
    uint8_t value = 0;
    
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    
    // Send address with read bit
    uint8_t addr = (reg & 0x3F) | CC1101_READ_SINGLE;
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &addr, 1, CC1101_SPI_TIMEOUT);
    furi_hal_spi_bus_rx(CC1101_SPI_HANDLE, &value, 1, CC1101_SPI_TIMEOUT);
    
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
    
    return value;
}

// Burst write
void cc1101_write_burst(uint8_t reg, const uint8_t* data, uint8_t len) {
    if(len == 0) return;
    
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    
    // Send address with burst write bit
    uint8_t addr = (reg & 0x3F) | CC1101_WRITE_BURST;
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &addr, 1, CC1101_SPI_TIMEOUT);
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, data, len, CC1101_SPI_TIMEOUT);
    
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

// Burst read
void cc1101_read_burst(uint8_t reg, uint8_t* data, uint8_t len) {
    if(len == 0) return;
    
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    
    // Send address with burst read bit
    uint8_t addr = (reg & 0x3F) | CC1101_READ_BURST;
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &addr, 1, CC1101_SPI_TIMEOUT);
    furi_hal_spi_bus_rx(CC1101_SPI_HANDLE, data, len, CC1101_SPI_TIMEOUT);
    
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

// Send command strobe
void cc1101_send_command(uint8_t cmd) {
    furi_mutex_acquire(spi_mutex, FuriWaitForever);
    furi_hal_spi_acquire(CC1101_SPI_HANDLE);
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &cmd, 1, CC1101_SPI_TIMEOUT);
    
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    furi_delay_us(1);
    
    furi_hal_spi_release(CC1101_SPI_HANDLE);
    furi_mutex_release(spi_mutex);
}

// Get status
CC1101Status_t cc1101_get_status(void) {
    CC1101Status_t status;
    
    status.partnum = cc1101_read_register(CC1101_PARTNUM);
    status.version = cc1101_read_register(CC1101_VERSION);
    status.rssi = cc1101_read_register(CC1101_RSSI);
    status.lqi = cc1101_read_register(CC1101_LQI);
    status.marcstate = cc1101_read_register(CC1101_MARCSTATE);
    status.pktstatus = cc1101_read_register(CC1101_PKTSTATUS);
    status.rxbytes = cc1101_read_register(CC1101_RXBYTES) & 0x7F;
    status.txbytes = cc1101_read_register(CC1101_TXBYTES) & 0x7F;
    
    return status;
}

// Get current state machine state
uint8_t cc1101_get_state(void) {
    return cc1101_read_register(CC1101_MARCSTATE) & 0x1F;
}

// Set frequency (in Hz)
void cc1101_set_frequency(uint32_t freq_hz) {
    // Calculate frequency word: freq_word = freq_hz / (26MHz / 2^16)
    // = freq_hz * 65536 / 26000000
    uint32_t freq_word = ((uint64_t)freq_hz * 65536ULL) / 26000000ULL;
    
    uint8_t freq2 = (freq_word >> 16) & 0xFF;
    uint8_t freq1 = (freq_word >> 8) & 0xFF;
    uint8_t freq0 = freq_word & 0xFF;
    
    cc1101_write_register(CC1101_FREQ2, freq2);
    cc1101_write_register(CC1101_FREQ1, freq1);
    cc1101_write_register(CC1101_FREQ0, freq0);
    
    current_config.frequency_hz = freq_hz;
    
    FURI_LOG_D(TAG, "Frequency set to %lu Hz (word: 0x%02X%02X%02X)", 
               freq_hz, freq2, freq1, freq0);
}

// Set data rate (in baud)
void cc1101_set_data_rate(uint32_t baud) {
    // Calculate data rate: DRATE = (baud * 2^28) / 26MHz
    // MDMCFG3 = DRATE_M, MDMCFG4 contains DRATE_E
    uint32_t drate = ((uint64_t)baud << 28) / 26000000ULL;
    uint8_t drate_e = 0;
    uint32_t drate_m = drate;
    
    while(drate_m > 255 && drate_e < 15) {
        drate_m >>= 1;
        drate_e++;
    }
    
    uint8_t mdmcfg4 = cc1101_read_register(CC1101_MDMCFG4);
    mdmcfg4 = (mdmcfg4 & 0xF0) | drate_e;
    
    cc1101_write_register(CC1101_MDMCFG4, mdmcfg4);
    cc1101_write_register(CC1101_MDMCFG3, (uint8_t)drate_m);
    
    current_config.data_rate = baud;
    
    FURI_LOG_D(TAG, "Data rate set to %lu baud (E=%d, M=%d)", baud, drate_e, (uint8_t)drate_m);
}

// Set modulation type
void cc1101_set_modulation(uint8_t modulation) {
    uint8_t mdmcfg2 = cc1101_read_register(CC1101_MDMCFG2);
    
    // Clear modulation bits (bits 4-6)
    mdmcfg2 &= 0x8F;
    
    switch(modulation) {
        case MOD_2FSK:
            mdmcfg2 |= 0x00;  // 2-FSK
            break;
        case MOD_GFSK:
            mdmcfg2 |= 0x10;  // GFSK
            break;
        case MOD_ASK:
        case MOD_OOK:
            mdmcfg2 |= 0x30;  // ASK/OOK
            break;
        case MOD_4FSK:
            mdmcfg2 |= 0x40;  // 4-FSK
            break;
        case MOD_MSK:
            mdmcfg2 |= 0x70;  // MSK
            break;
        default:
            FURI_LOG_W(TAG, "Unknown modulation type: %d", modulation);
            return;
    }
    
    cc1101_write_register(CC1101_MDMCFG2, mdmcfg2);
    current_config.modulation = modulation;
    
    FURI_LOG_D(TAG, "Modulation set to %d", modulation);
}

// Set TX power (-20 to +10 dBm)
void cc1101_set_tx_power(uint8_t power_dbm) {
    // Convert dBm to PA table index
    // CC1101 PA table: -30, -20, -15, -10, -6, 0, +5, +7, +10, +12 dBm
    uint8_t pa_table[8] = {0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    if(power_dbm <= 10) {
        // Use -20 to +10 dBm range
        pa_table[0] = 0x50;  // -10 dBm base
        // Adjust for requested power
        if(power_dbm >= 10) pa_table[0] = 0xC0;      // +10 dBm
        else if(power_dbm >= 7) pa_table[0] = 0xC8; // +7 dBm
        else if(power_dbm >= 5) pa_table[0] = 0x84;  // +5 dBm
        else if(power_dbm >= 0) pa_table[0] = 0x60;  // 0 dBm
        else if(power_dbm >= -6) pa_table[0] = 0x50; // -6 dBm
        else if(power_dbm >= -10) pa_table[0] = 0x34; // -10 dBm
        else pa_table[0] = 0x12;  // -20 dBm
    }
    
    cc1101_write_burst(CC1101_PATABLE, pa_table, 8);
    current_config.tx_power = power_dbm;
    
    FURI_LOG_D(TAG, "TX power set to %d dBm", power_dbm);
}

// Set channel number
void cc1101_set_channel(uint8_t channel) {
    cc1101_write_register(CC1101_CHANNR, channel);
}

// Enter RX mode
void cc1101_enter_rx(void) {
    cc1101_send_command(CC1101_SRX);
    // Wait for RX state
    uint8_t state;
    uint32_t timeout = 1000;
    do {
        state = cc1101_get_state();
        furi_delay_us(10);
    } while(state != CC1101_STATE_RX && --timeout > 0);
}

// Enter TX mode
void cc1101_enter_tx(void) {
    cc1101_send_command(CC1101_STX);
    // Wait for TX state
    uint8_t state;
    uint32_t timeout = 1000;
    do {
        state = cc1101_get_state();
        furi_delay_us(10);
    } while(state != CC1101_STATE_TX && --timeout > 0);
}

// Enter idle mode
void cc1101_enter_idle(void) {
    cc1101_send_command(CC1101_SIDLE);
    uint8_t state;
    uint32_t timeout = 1000;
    do {
        state = cc1101_get_state();
        furi_delay_us(10);
    } while(state != CC1101_STATE_IDLE && --timeout > 0);
}

// Flush RX FIFO
void cc1101_flush_rx(void) {
    cc1101_send_command(CC1101_SFRX);
}

// Flush TX FIFO
void cc1101_flush_tx(void) {
    cc1101_send_command(CC1101_SFTX);
}

// Read RSSI from register
uint8_t cc1101_read_rssi(void) {
    return cc1101_read_register(CC1101_RSSI);
}

// Read RSSI directly (for ISR use)
uint8_t cc1101_read_rssi_live(void) {
    // Direct SPI read without mutex for ISR safety
    uint8_t addr = (CC1101_RSSI & 0x3F) | CC1101_READ_SINGLE;
    uint8_t value = 0;
    
    furi_hal_gpio_write(CC1101_CS_PIN, false);
    furi_delay_us(1);
    furi_hal_spi_bus_tx(CC1101_SPI_HANDLE, &addr, 1, CC1101_SPI_TIMEOUT);
    furi_hal_spi_bus_rx(CC1101_SPI_HANDLE, &value, 1, CC1101_SPI_TIMEOUT);
    furi_hal_gpio_write(CC1101_CS_PIN, true);
    
    return value;
}

// Convert RSSI to dBm
int16_t cc1101_rssi_to_dbm(uint8_t rssi_reg) {
    if(rssi_reg >= 128) {
        return (int16_t)rssi_reg - 256;
    } else {
        return (int16_t)rssi_reg - 256;
    }
}

// Receive packet
bool cc1101_receive_packet(uint8_t* data, uint8_t* len, uint8_t* rssi, uint8_t* lqi) {
    uint8_t rxbytes = cc1101_read_register(CC1101_RXBYTES);
    
    // Check for overflow
    if(rxbytes & 0x80) {
        FURI_LOG_W(TAG, "RX FIFO overflow");
        cc1101_flush_rx();
        return false;
    }
    
    uint8_t pktlen = rxbytes & 0x7F;
    if(pktlen == 0) {
        return false;  // No data available
    }
    
    // Read packet length (first byte in FIFO)
    uint8_t fifo_data[64];
    cc1101_read_burst(CC1101_RXFIFO, fifo_data, (pktlen > 64) ? 64 : pktlen);
    
    // First byte is length for variable length mode
    uint8_t actual_len = fifo_data[0];
    if(actual_len > 60) actual_len = 60;  // Safety limit
    
    // Copy data (skip length byte)
    memcpy(data, &fifo_data[1], actual_len);
    *len = actual_len;
    
    // Read RSSI and LQI (last two bytes in FIFO)
    if(rssi) *rssi = fifo_data[pktlen - 2];
    if(lqi) *lqi = fifo_data[pktlen - 1];
    
    return true;
}

// Transmit packet
bool cc1101_transmit_packet(const uint8_t* data, uint8_t len) {
    if(len > 60) {
        FURI_LOG_E(TAG, "Packet too long: %d bytes", len);
        return false;
    }
    
    // Flush TX FIFO
    cc1101_flush_tx();
    
    // Write packet to FIFO (length byte + data)
    uint8_t tx_data[65];
    tx_data[0] = len;
    memcpy(&tx_data[1], data, len);
    
    cc1101_write_burst(CC1101_TXFIFO, tx_data, len + 1);
    
    // Enter TX mode
    cc1101_enter_tx();
    
    // Wait for transmission complete
    uint8_t state;
    uint32_t timeout = 10000;  // 100ms max
    do {
        state = cc1101_get_state();
        furi_delay_us(10);
    } while(state == CC1101_STATE_TX && --timeout > 0);
    
    if(timeout == 0) {
        FURI_LOG_E(TAG, "TX timeout");
        cc1101_enter_idle();
        return false;
    }
    
    return true;
}

// Set low power mode
void cc1101_set_low_power_mode(bool enable) {
    low_power_mode = enable;
    
    if(enable) {
        // Reduce RX duty cycle
        uint8_t mcsm2 = cc1101_read_register(CC1101_MCSM2);
        mcsm2 |= 0x07;  // RX_TIME = 7 (longer sleep)
        cc1101_write_register(CC1101_MCSM2, mcsm2);
        
        FURI_LOG_I(TAG, "Low power mode enabled");
    } else {
        // Restore normal RX duty cycle
        uint8_t mcsm2 = cc1101_read_register(CC1101_MCSM2);
        mcsm2 &= ~0x07;  // RX_TIME = 0 (continuous)
        cc1101_write_register(CC1101_MCSM2, mcsm2);
        
        FURI_LOG_I(TAG, "Low power mode disabled");
    }
}

// Calibrate frequency synthesizer
void cc1101_calibrate(void) {
    cc1101_send_command(CC1101_SCAL);
    furi_delay_us(CC1101_CALIBRATE_TIME_US);
    
    FURI_LOG_I(TAG, "Frequency synthesizer calibrated");
}

// Set sync word
void cc1101_set_sync_word(const uint8_t* sync_word) {
    cc1101_write_register(CC1101_SYNC1, sync_word[0]);
    cc1101_write_register(CC1101_SYNC0, sync_word[1]);
    
    current_config.sync_word[0] = sync_word[0];
    current_config.sync_word[1] = sync_word[1];
}

// Set preamble length
void cc1101_set_preamble(uint8_t preamble_bytes) {
    uint8_t mdmcfg1 = cc1101_read_register(CC1101_MDMCFG1);
    mdmcfg1 &= 0x8F;  // Clear preamble bits
    // Preamble bytes: 0=2, 1=3, 2=4, 3=6, 4=8, 5=12, 6=16, 7=24
    uint8_t preamble_cfg = 0;
    if(preamble_bytes >= 24) preamble_cfg = 7;
    else if(preamble_bytes >= 16) preamble_cfg = 6;
    else if(preamble_bytes >= 12) preamble_cfg = 5;
    else if(preamble_bytes >= 8) preamble_cfg = 4;
    else if(preamble_bytes >= 6) preamble_cfg = 3;
    else if(preamble_bytes >= 4) preamble_cfg = 2;
    else if(preamble_bytes >= 3) preamble_cfg = 1;
    
    mdmcfg1 |= (preamble_cfg << 4);
    cc1101_write_register(CC1101_MDMCFG1, mdmcfg1);
}

// Load preset configuration
void cc1101_load_preset_config(const uint8_t* config) {
    // Write configuration registers 0x00-0x1F
    cc1101_write_burst(0x00, config, 32);
    
    FURI_LOG_I(TAG, "Preset configuration loaded");
}

// Start RSSI sampling for fingerprinting
void cc1101_start_rssi_sampling(uint16_t sample_rate_hz) {
    // Configure GDO0 to output continuous RSSI
    // This would require specific register configuration
    // For now, we'll use burst reads in the capture loop
    UNUSED(sample_rate_hz);
    
    FURI_LOG_I(TAG, "RSSI sampling started at %d Hz", sample_rate_hz);
}

void cc1101_stop_rssi_sampling(void) {
    // Restore normal GDO0 configuration
    FURI_LOG_I(TAG, "RSSI sampling stopped");
}

uint8_t cc1101_get_rssi_sample(void) {
    return last_rssi;
}

// Frequency hopping
static volatile bool freq_hopping_enabled = false;
static volatile uint16_t hop_interval_ms = 100;
static volatile uint32_t current_freq_index = 0;
static uint32_t hop_frequencies[16];
static uint8_t num_hop_freqs = 0;

void cc1101_set_frequency_hopping(bool enable, uint16_t interval_ms) {
    freq_hopping_enabled = enable;
    hop_interval_ms = interval_ms;
    
    if(enable) {
        FURI_LOG_I(TAG, "Frequency hopping enabled, interval: %d ms", interval_ms);
    } else {
        FURI_LOG_I(TAG, "Frequency hopping disabled");
    }
}

void cc1101_set_hop_frequencies(const uint32_t* freqs, uint8_t count) {
    if(count > 16) count = 16;
    memcpy(hop_frequencies, freqs, count * sizeof(uint32_t));
    num_hop_freqs = count;
    current_freq_index = 0;
}

void cc1101_hop_frequency(void) {
    if(!freq_hopping_enabled || num_hop_freqs == 0) return;
    
    current_freq_index = (current_freq_index + 1) % num_hop_freqs;
    cc1101_set_frequency(hop_frequencies[current_freq_index]);
}
