# Flipper RF Lab - User Guide

## Table of Contents
- [Getting Started](#getting-started)
- [Main Menu](#main-menu)
- [Capture Mode](#capture-mode)
- [Spectrum Scan](#spectrum-scan)
- [Device Database](#device-database)
- [Research Mode](#research-mode)
- [Settings](#settings)
- [Tips & Tricks](#tips--tricks)

## Getting Started

### First Launch
When you first open Flipper RF Lab, you'll see the main menu with four options:

```
┌─────────────────────────────┐
│  🔬 Flipper RF Lab v1.0     │
├─────────────────────────────┤
│  > Capture Mode             │
│    Spectrum Scan            │
│    Device Database          │
│    Research Mode            │
│    Settings                 │
└─────────────────────────────┘
```

### Navigation
- **↑/↓ (D-Pad)**: Navigate menu items
- **OK (Center)**: Select/Confirm
- **← (Back)**: Go back/Cancel
- **Long Press OK**: Context menu

## Main Menu

### 1. Capture Mode
Real-time RF signal capture with automatic analysis.

**Features:**
- Automatic protocol detection
- Device fingerprinting
- Threat assessment
- Signal recording to SD card

**How to use:**
1. Select **Capture Mode**
2. Choose frequency band or enter custom frequency
3. Select modulation type (or Auto-detect)
4. Press **OK** to start capture
5. Trigger the target device (press key fob, etc.)
6. View real-time analysis results

**Display Elements:**
```
┌─────────────────────────────┐
│ 433.92 MHz  OOK  -72 dBm    │
├─────────────────────────────┤
│ ████████░░░░░░░░░░░░░░░░░░░ │  <- RSSI bar
│                             │
│ Device: Key Fob (95% match) │
│ Protocol: PT2262 (guess)    │
│ Threat: HIGH (static code)  │
│                             │
│ [OK] Save  [←] Back         │
└─────────────────────────────┘
```

### 2. Spectrum Scan
Continuous 300-928 MHz spectrum monitoring.

**Features:**
- Real-time waterfall display
- Peak hold and average traces
- Activity logging
- Protocol detection markers

**How to use:**
1. Select **Spectrum Scan**
2. Choose scan range (Full/Custom)
3. Adjust dwell time (10-100ms per step)
4. View real-time spectrum

**Controls:**
- **↑/↓**: Adjust sensitivity threshold
- **←/→**: Scroll frequency range
- **OK**: Mark frequency for capture
- **Long OK**: Pause/Resume scan

### 3. Device Database
View and manage identified device fingerprints.

**Features:**
- List all captured devices
- View device details
- Compare fingerprints
- Export to SD card

**Database Fields:**
- Device ID (auto-generated)
- First seen / Last seen timestamps
- Frequency and modulation
- Fingerprint signature
- Risk assessment
- Capture count

### 4. Research Mode
Advanced tools for protocol reverse engineering.

**Sub-menus:**
- **Raw Dump**: Export timing to CSV
- **Frame View**: Hex/ASCII inspection
- **Histogram**: Pulse width distribution
- **Alignment**: Manual bit framing

#### Raw Dump
Export pulse timings for external analysis.

**Output format (CSV):**
```csv
timestamp_us,level,duration_us
0,1,520
520,0,1040
1560,1,520
...
```

#### Frame View
Inspect captured frames at bit level.

**Navigation:**
- **↑/↓**: Next/Previous frame
- **←/→**: Navigate bits
- **OK**: Toggle hex/ASCII
- **Long OK**: Search pattern

#### Histogram
Visualize pulse width distribution.

**Use for:**
- Identifying symbol types
- Determining encoding scheme
- Finding optimal thresholds

### 5. Settings
Configure RF parameters and application options.

**Settings Menu:**
```
┌─────────────────────────────┐
│  Settings                   │
├─────────────────────────────┤
│  Frequency Band...          │
│  Modulation...              │
│  Data Rate...               │
│  TX Power...                │
│  SD Card...                 │
│  Display...                 │
│  System Info                │
│  Reset to Defaults          │
└─────────────────────────────┘
```

#### Frequency Band
- **315 MHz** (Asia, some US devices)
- **433.92 MHz** (Europe, most common)
- **868 MHz** (Europe, higher power)
- **915 MHz** (US/Asia)
- **Custom** (300-928 MHz)

#### Modulation
- **Auto-detect** (recommended)
- **OOK** (On-Off Keying)
- **ASK** (Amplitude Shift Keying)
- **FSK** (Frequency Shift Keying)
- **GFSK** (Gaussian FSK)

#### Data Rate
Common presets:
- **0.6 kBaud** (long range, slow)
- **1.2 kBaud** (key fobs)
- **2.4 kBaud** (common default)
- **9.6 kBaud** (faster devices)
- **Custom** (0.6-500 kBaud)

#### TX Power
- **-20 dBm** (0.01 mW) - Very low
- **-10 dBm** (0.1 mW) - Low
- **0 dBm** (1 mW) - Medium
- **+7 dBm** (5 mW) - High
- **+10 dBm** (10 mW) - Maximum

⚠️ **Warning**: Check local regulations before transmitting!

#### SD Card Options
- **Auto-save captures**: ON/OFF
- **Compression**: Delta/RLE/None
- **Rolling buffer size**: 100MB-1GB
- **Export format**: CSV/JSON/Binary

#### Display Options
- **Brightness**: 1-5
- **Contrast**: Adjust for visibility
- **Screensaver**: 30s/1m/5m/Never
- **Scroll speed**: Fast/Normal/Slow

## Tips & Tricks

### Improving Capture Quality

1. **Antenna Positioning**
   - Keep antenna vertical for omnidirectional devices
   - Move closer to reduce interference
   - Avoid metal objects nearby

2. **Frequency Selection**
   - Start with known frequency (check device label)
   - Use Spectrum Scan to find active frequencies
   - Try harmonics if main frequency fails

3. **Timing Optimization**
   - Reduce data rate for weak signals
   - Increase for noisy environments
   - Match device specification if known

### Common Device Frequencies

| Device Type | Typical Frequency |
|-------------|-----------------|
| Car key fob | 315 or 433.92 MHz |
| Garage door | 433.92 MHz |
| Weather station | 433.92 or 868 MHz |
| Wireless doorbell | 433.92 MHz |
| Remote switch | 433.92 MHz |
| Tire pressure sensor | 315 or 433.92 MHz |
| Smart home sensor | 868 or 915 MHz |

### Battery Optimization

For extended logging sessions:
1. Reduce screen brightness
2. Enable screensaver
3. Use passive scan mode
4. Disable auto-save if not needed
5. Lower sample rate for background monitoring

Expected battery life:
- **Active capture**: 6-8 hours
- **Passive scan**: 12-16 hours
- **Standby logging**: 48+ hours

### Data Export

Captured data is stored on SD card:
```
/ext/apps_data/flipper_rf_lab/
├── captures/
│   ├── session_001/
│   │   ├── raw/
│   │   ├── analyzed/
│   │   └── metadata.json
├── fingerprints/
│   └── device_db.bin
├── logs/
│   ├── system.log
│   └── activity_2024-01-15.csv
└── exports/
    └── analysis_report.txt
```

### Safety & Legal

⚠️ **Important:**
- Only transmit on frequencies you're licensed to use
- Check local RF regulations
- Don't capture sensitive data without permission
- Some frequencies require licenses (PMR, amateur radio)

### Keyboard Shortcuts

| Key | Function |
|-----|----------|
| ↑ | Navigate up / Increase value |
| ↓ | Navigate down / Decrease value |
| ← | Go back / Previous |
| → | Select / Next |
| OK | Confirm / Action |
| Long OK | Context menu / Special action |
| Back | Cancel / Return |

### Getting Help

- **In-app**: Settings → About → Help
- **Online**: [GitHub Issues](https://github.com/tworjaga/flipper-rf-lab/issues)
- **Community**: [Flipper Discord](https://discord.gg/flipper)
- **Documentation**: [Full Docs](https://github.com/tworjaga/flipper-rf-lab/tree/main/docs)

---

**Happy RF Hacking!** 🔬📡
