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

#define CC1101_IOCFG2       0x00
#define CC1101_IOCFG1       0x01
#define CC1101_IOCFG0       0x02
#define CC1101_FIFOTHR      0x03
#define CC1101_SYNC1        0x04
#define CC1101_SYNC0        0x05
#define CC1101_PKTLEN       0x06
#define CC1101_PKTCTRL1     0x07
#define CC1101_PKTCTRL0     0x08
#define CC1101_ADDR         0x09
#define CC1101_CHANNR       0x0A
#define CC1101_FSCTRL1      0x0B
#define CC1101_FSCTRL0      0x0C
#define CC1101_FREQ2        0x0D
#define CC1101_FREQ1        0x0E
#define CC1101_FREQ0        0x0F
#define CC1101_MDMCFG4      0x10
#define CC1101_MDMCFG3      0x11
#define CC1101_MDMCFG2      0x12
#define CC1101_MDMCFG1      0x13
#define CC1101_MDMCFG0      0x14
#define CC1101_DEVIATN      0x15
#define CC1101_MCSM2        0x16
#define CC1101_MCSM1        0x17
#define CC1101_MCSM0        0x18
#define CC1101_FOCCFG       0x19
#define CC1101_BSCFG        0x1A
#define CC1101_AGCCTRL2     0x1B
#define CC1101_AGCCTRL1     0x1C
#define CC1101_AGCCTRL0     0x1D
#define CC1101_WOREVT1      0x1E
#define CC1101_WOREVT0      0x1F
#define CC1101_WORCTRL      0x20
#define CC1101_FREND1       0x21
#define CC1101_FREND0       0x22
#define CC1101_FSCAL3       0x23
#define CC1101_FSCAL2       0x24
#define CC1101_FSCAL1       0x25
#define CC1101_FSCAL0       0x26
#define CC1101_RCCTRL1      0x27
#define CC1101_RCCTRL0      0x28
#define CC1101_FSTEST       0x29
#define CC1101_PTEST        0x2A
#define CC1101_AGCTEST      0x2B
#define CC1101_TEST2        0x2C
#define CC1101_TEST1        0x2D
#define CC1101_TEST0        0x2E

// Command strobes
#define CC1101_SRES         0x30
#define CC1101_SFSTXON      0x31
#define CC1101_SXOFF        0x32
#define CC1101_SCAL         0x33
#define CC1101_SRX          0x34
#define CC1101_STX          0x35
#define CC1101_SIDLE        0x36
#define CC1101_SWOR         0x38
#define CC1101_SPWD         0x39
#define CC1101_SFRX         0x3A
#define CC1101_SFTX         0x3B
#define CC1101_SWORRST      0x3C
#define CC1101_SNOP         0x3D

// Status registers (burst-read address | 0xC0)
#define CC1101_PARTNUM      0x30
#define CC1101_VERSION      0x31
#define CC1101_FREQEST      0x32
#define CC1101_LQI          0x33
#define CC1101_RSSI         0x34
#define CC1101_MARCSTATE    0x35
#define CC1101_WORTIME1     0x36
#define CC1101_WORTIME0     0x37
#define CC1101_PKTSTATUS    0x38
#define CC1101_VCO_VC_DAC   0x39
#define CC1101_TXBYTES      0x3A
#define CC1101_RXBYTES      0x3B
#define CC1101_RCCTRL1_STATUS 0x3C
#define CC1101_RCCTRL0_STATUS 0x3D

// FIFO addresses
#define CC1101_TXFIFO       0x3F
#define CC1101_RXFIFO       0x3F

// Burst access bits
#define CC1101_WRITE_BURST  0x40
#define CC1101_READ_SINGLE  0x80
#define CC1101_READ_BURST   0xC0

// State machine states (MARCSTATE register bits[4:0])
#define CC1101_STATE_IDLE        0x01
#define CC1101_STATE_RX          0x0D
#define CC1101_STATE_TX          0x13
#define CC1101_STATE_FSTXON      0x12
#define CC1101_STATE_CALIBRATE   0x08
#define CC1101_STATE_SETTLING    0x09
#define CC1101_STATE_RX_OVERFLOW 0x11
#define CC1101_STATE_TX_UNDERFLOW 0x22

// ============================================================================
// CC1101 CONFIGURATION STRUCTURES
// ============================================================================

typedef struct {
    uint32_t frequency_hz;
    uint32_t data_rate;
    uint8_t  modulation;
    int8_t   tx_power;      // signed: -20 to +12 dBm
    uint32_t channel_bw;
    uint8_t  sync_word[2];
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

FuriStatus cc1101_driver_init(void);
void       cc1101_driver_deinit(void);

void    cc1101_reset(void);

// Locked (mutex + bus): safe to call from any thread
void    cc1101_write_register(uint8_t reg, uint8_t value);
uint8_t cc1101_read_register(uint8_t reg);
void    cc1101_write_burst(uint8_t reg, const uint8_t* data, uint8_t len);
void    cc1101_read_burst(uint8_t reg, uint8_t* data, uint8_t len);
void    cc1101_send_command(uint8_t cmd);

CC1101Status_t cc1101_get_status(void);
uint8_t        cc1101_get_state(void);

void    cc1101_set_frequency(uint32_t freq_hz);
void    cc1101_set_data_rate(uint32_t baud);
void    cc1101_set_modulation(uint8_t modulation);
// FIX: parameter is int8_t to correctly represent -20..+12 dBm
void    cc1101_set_tx_power(int8_t power_dbm);
void    cc1101_set_channel(uint8_t channel);

void    cc1101_enter_rx(void);
void    cc1101_enter_tx(void);
void    cc1101_enter_idle(void);
void    cc1101_flush_rx(void);
void    cc1101_flush_tx(void);

uint8_t cc1101_read_rssi(void);
// FIX: removed cc1101_read_rssi_live() — was bypassing the SPI mutex.
//      Call cc1101_read_rssi() from thread context instead.

int16_t cc1101_rssi_to_dbm(uint8_t rssi_reg);

bool    cc1101_receive_packet(uint8_t* data, uint8_t* len, uint8_t* rssi, uint8_t* lqi);
bool    cc1101_transmit_packet(const uint8_t* data, uint8_t len);
bool    cc1101_has_data(void);

void    cc1101_set_low_power_mode(bool enable);
void    cc1101_calibrate(void);
void    cc1101_set_sync_word(const uint8_t* sync_word);
void    cc1101_set_preamble(uint8_t preamble_bytes);
void    cc1101_load_preset_config(const uint8_t* config);

// ISR-driven RSSI: call cc1101_service_rssi_pending() from thread context only
void    cc1101_service_rssi_pending(void);
uint8_t cc1101_get_rssi_sample(void);

void    cc1101_start_rssi_sampling(uint16_t sample_rate_hz);
void    cc1101_stop_rssi_sampling(void);

void    cc1101_set_frequency_hopping(bool enable, uint16_t hop_interval_ms);
void    cc1101_set_hop_frequencies(const uint32_t* freqs, uint8_t count);
void    cc1101_hop_frequency(void);

// ============================================================================
// PRESET CONFIGURATIONS
// ============================================================================

extern const uint8_t CC1101_CONFIG_433_OOK[];
extern const uint8_t CC1101_CONFIG_868_FSK[];
extern const uint8_t CC1101_CONFIG_915_GFSK[];
extern const uint8_t CC1101_CONFIG_315_ASK[];

#ifdef __cplusplus
}
#endif

#endif // CC1101_DRIVER_H
