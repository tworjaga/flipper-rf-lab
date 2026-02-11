#ifndef CC1101_DRIVER_H
#define CC1101_DRIVER_H

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include "../flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CC1101 REGISTER DEFINITIONS
// ============================================================================

// Configuration registers
#define CC1101_IOCFG2       0x00    // GDO2 output pin configuration
#define CC1101_IOCFG1       0x01    // GDO1 output pin configuration
#define CC1101_IOCFG0       0x02    // GDO0 output pin configuration
#define CC1101_FIFOTHR      0x03    // RX FIFO and TX FIFO thresholds
#define CC1101_SYNC1        0x04    // Sync word, high byte
#define CC1101_SYNC0        0x05    // Sync word, low byte
#define CC1101_PKTLEN       0x06    // Packet length
#define CC1101_PKTCTRL1     0x07    // Packet automation control
#define CC1101_PKTCTRL0     0x08    // Packet automation control
#define CC1101_ADDR         0x09    // Device address
#define CC1101_CHANNR       0x0A    // Channel number
#define CC1101_FSCTRL1      0x0B    // Frequency synthesizer control
#define CC1101_FSCTRL0      0x0C    // Frequency synthesizer control
#define CC1101_FREQ2        0x0D    // Frequency control word, high byte
#define CC1101_FREQ1        0x0E    // Frequency control word, middle byte
#define CC1101_FREQ0        0x0F    // Frequency control word, low byte
#define CC1101_MDMCFG4      0x10    // Modem configuration
#define CC1101_MDMCFG3      0x11    // Modem configuration
#define CC1101_MDMCFG2      0x12    // Modem configuration
#define CC1101_MDMCFG1      0x13    // Modem configuration
#define CC1101_MDMCFG0      0x14    // Modem configuration
#define CC1101_DEVIATN      0x15    // Modem deviation setting
#define CC1101_MCSM2        0x16    // Main radio control state machine config
#define CC1101_MCSM1        0x17    // Main radio control state machine config
#define CC1101_MCSM0        0x18    // Main radio control state machine config
#define CC1101_FOCCFG       0x19    // Frequency offset compensation config
#define CC1101_BSCFG        0x1A    // Bit synchronization configuration
#define CC1101_AGCCTRL2     0x1B    // AGC control
#define CC1101_AGCCTRL1     0x1C    // AGC control
#define CC1101_AGCCTRL0     0x1D    // AGC control
#define CC1101_WOREVT1      0x1E    // High byte event0 timeout
#define CC1101_WOREVT0      0x1F    // Low byte event0 timeout
#define CC1101_WORCTRL      0x20    // Wake on radio control
#define CC1101_FREND1       0x21    // Front end RX configuration
#define CC1101_FREND0       0x22    // Front end TX configuration
#define CC1101_FSCAL3       0x23    // Frequency synthesizer calibration
#define CC1101_FSCAL2       0x24    // Frequency synthesizer calibration
#define CC1101_FSCAL1       0x25    // Frequency synthesizer calibration
#define CC1101_FSCAL0       0x26    // Frequency synthesizer calibration
#define CC1101_RCCTRL1      0x27    // RC oscillator configuration
#define CC1101_RCCTRL0      0x28    // RC oscillator configuration
#define CC1101_FSTEST       0x29    // Frequency synthesizer calibration control
#define CC1101_PTEST        0x2A    // Production test
#define CC1101_AGCTEST      0x2B    // AGC test
#define CC1101_TEST2        0x2C    // Various test settings
#define CC1101_TEST1        0x2D    // Various test settings
#define CC1101_TEST0        0x2E    // Various test settings

// Command strobes
#define CC1101_SRES         0x30    // Reset chip
#define CC1101_SFSTXON      0x31    // Enable and calibrate frequency synthesizer
#define CC1101_SXOFF        0x32    // Turn off crystal oscillator
#define CC1101_SCAL         0x33    // Calibrate frequency synthesizer and turn it off
#define CC1101_SRX          0x34    // Enable RX
#define CC1101_STX          0x35    // Enable TX
#define CC1101_SIDLE        0x36    // Exit RX / TX, turn off frequency synthesizer
#define CC1101_SWOR         0x38    // Start automatic RX polling sequence
#define CC1101_SPWD         0x39    // Enter power down mode when CSn goes high
#define CC1101_SFRX         0x3A    // Flush the RX FIFO buffer
#define CC1101_SFTX         0x3B    // Flush the TX FIFO buffer
#define CC1101_SWORRST      0x3C    // Reset real time clock
#define CC1101_SNOP         0x3D    // No operation

// Status registers
#define CC1101_PARTNUM      0x30    // Part number
#define CC1101_VERSION      0x31    // Current version number
#define CC1101_FREQEST      0x32    // Frequency offset estimate
#define CC1101_LQI          0x33    // Demodulator estimate for link quality
#define CC1101_RSSI         0x34    // Received signal strength indication
#define CC1101_MARCSTATE    0x35    // Control state machine state
#define CC1101_WORTIME1     0x36    // High byte of WOR timer
#define CC1101_WORTIME0     0x37    // Low byte of WOR timer
#define CC1101_PKTSTATUS    0x38    // Current GDOx status and packet status
#define CC1101_VCO_VC_DAC   0x39    // Current setting from PLL calibration module
#define CC1101_TXBYTES      0x3A    // Underflow and number of bytes in TXFIFO
#define CC1101_RXBYTES      0x3B    // Overflow and number of bytes in RXFIFO
#define CC1101_RCCTRL1_STATUS 0x3C  // Last RC oscillator calibration result
#define CC1101_RCCTRL0_STATUS 0x3D  // Last RC oscillator calibration result

// Burst access bits
#define CC1101_WRITE_BURST    0x40
#define CC1101_READ_SINGLE    0x80
#define CC1101_READ_BURST     0xC0

// Status byte states
#define CC1101_STATE_IDLE     0x00
#define CC1101_STATE_RX      0x10
#define CC1101_STATE_TX      0x20
#define CC1101_STATE_FSTXON  0x30
#define CC1101_STATE_CALIBRATE 0x40
#define CC1101_STATE_SETTLING 0x50
#define CC1101_STATE_RX_OVERFLOW 0x60
#define CC1101_STATE_TX_UNDERFLOW 0x70

// ============================================================================
// CC1101 CONFIGURATION STRUCTURES
// ============================================================================

typedef struct {
    uint32_t frequency_hz;
    uint32_t data_rate;
    uint8_t modulation;
    uint8_t tx_power;
    uint32_t channel_bw;
    uint8_t sync_word[2];
} CC1101Config_t;

typedef struct {
    uint8_t partnum;
    uint8_t version;
    uint8_t rssi;
    uint8_t lqi;
    uint8_t marcstate;
    uint8_t pktstatus;
    uint8_t rxbytes;
    uint8_t txbytes;
} CC1101Status_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization and deinitialization
FuriStatus cc1101_driver_init(void);
void cc1101_driver_deinit(void);

// Configuration
void cc1101_reset(void);
void cc1101_write_register(uint8_t reg, uint8_t value);
uint8_t cc1101_read_register(uint8_t reg);
void cc1101_write_burst(uint8_t reg, const uint8_t* data, uint8_t len);
void cc1101_read_burst(uint8_t reg, uint8_t* data, uint8_t len);
void cc1101_send_command(uint8_t cmd);

// Status and control
CC1101Status_t cc1101_get_status(void);
uint8_t cc1101_get_state(void);
void cc1101_set_frequency(uint32_t freq_hz);
void cc1101_set_data_rate(uint32_t baud);
void cc1101_set_modulation(uint8_t modulation);
void cc1101_set_tx_power(uint8_t power_dbm);
void cc1101_set_channel(uint8_t channel);

// RX/TX operations
void cc1101_enter_rx(void);
void cc1101_enter_tx(void);
void cc1101_enter_idle(void);
void cc1101_flush_rx(void);
void cc1101_flush_tx(void);

// Data transfer
uint8_t cc1101_read_rssi(void);
uint8_t cc1101_read_rssi_live(void);
bool cc1101_receive_packet(uint8_t* data, uint8_t* len, uint8_t* rssi, uint8_t* lqi);
bool cc1101_transmit_packet(const uint8_t* data, uint8_t len);

// Advanced features
void cc1101_set_low_power_mode(bool enable);
void cc1101_calibrate(void);
void cc1101_set_sync_word(const uint8_t* sync_word);
void cc1101_set_preamble(uint8_t preamble_bytes);

// DMA operations (for high-speed transfers)
void cc1101_dma_init(void);
bool cc1101_dma_transfer(const uint8_t* tx_data, uint8_t* rx_data, uint16_t len);
void cc1101_dma_wait_complete(void);

// RSSI sampling for fingerprinting
void cc1101_start_rssi_sampling(uint16_t sample_rate_hz);
void cc1101_stop_rssi_sampling(void);
uint8_t cc1101_get_rssi_sample(void);

// Frequency hopping
void cc1101_set_frequency_hopping(bool enable, uint16_t hop_interval_ms);
void cc1101_hop_frequency(void);

// ============================================================================
// PRESET CONFIGURATIONS
// ============================================================================

// 433.92 MHz, 2.4 kbps, OOK (common for key fobs)
extern const uint8_t CC1101_CONFIG_433_OOK[];

// 868.35 MHz, 4.8 kbps, FSK (common for sensors)
extern const uint8_t CC1101_CONFIG_868_FSK[];

// 915 MHz, 38.4 kbps, GFSK (common for IoT)
extern const uint8_t CC1101_CONFIG_915_GFSK[];

// 315 MHz, 2 kbps, ASK (common for garage remotes)
extern const uint8_t CC1101_CONFIG_315_ASK[];

#ifdef __cplusplus
}
#endif

#endif // CC1101_DRIVER_H
