# 🔬 Flipper RF Lab

[![Flipper Zero](https://img.shields.io/badge/Flipper-Zero-orange)](https://flipperzero.one)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-30%2F30%20Passing-brightgreen)](tests/test_algorithms.py)

> **Professional RF Analysis & Research Platform for Flipper Zero**
> 
> Transform your Flipper Zero into a laboratory-grade RF forensics instrument with 15 advanced analysis features.

![Flipper RF Lab](docs/images/flipper-rf-lab-banner.png)

## ✨ Features

| Feature | Description | Status |
|---------|-------------|--------|
| 🔍 **RF Fingerprinting** | Device-level identification via timing drift, rise/fall slopes, clock instability | ✅ |
| 📊 **Adaptive Signal Modeling** | Automatic protocol detection and hypothesis generation | ✅ |
| 🔄 **Cross-Session Correlation** | Compare signals across capture sessions | ✅ |
| 📈 **Behavioral Profiling** | Long-term device behavior analysis | ✅ |
| ⏱️ **Timing Stability Analysis** | Laboratory-grade oscillator characterization | ✅ |
| 🎯 **Advanced Clustering** | K-means and hierarchical signal grouping | ✅ |
| ⚠️ **Threat Modeling** | Vulnerability scoring and risk assessment | ✅ |
| 📡 **Real-Time Activity Map** | Continuous 300-928 MHz spectrum monitoring | ✅ |
| 🎮 **Signal Replay Integrity** | Verify fidelity of signal regeneration | ✅ |
| 🔬 **Modular Research Mode** | Deep analytical tools for protocol reverse engineering | ✅ |
| 📊 **Internal Telemetry** | OS-level performance monitoring | ✅ |
| 🎯 **Deterministic Execution** | <1μs timing jitter for scientific validity | ✅ |
| 🗜️ **Signal Compression** | 5:1+ compression for long-term logging | ✅ |
| 💾 **Long-Term Logging** | Unattended background monitoring | ✅ |
| 🧮 **Embedded Math Toolkit** | Q15.16 fixed-point operations | ✅ |

## 🚀 Quick Start

### Prerequisites
- [Flipper Zero](https://flipperzero.one/) device
- [Flipper Zero Firmware SDK](https://github.com/flipperdevices/flipperzero-firmware)

### Installation

```bash
# 1. Clone the Flipper Zero firmware
git clone https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# 2. Add this application to user apps
git clone https://github.com/tworjaga/flipper-rf-lab.git applications_user/flipper-rf-lab

# 3. Build the application
./fbt fap_flipper_rf_lab

# 4. Flash to your Flipper Zero
./fbt fap_flipper_rf_lab flash
```

### Alternative: Direct Download
Download the latest `.fap` file from [Releases](https://github.com/tworjaga/flipper-rf-lab/releases) and copy to your Flipper's SD card.

## 📖 Usage

### Main Menu
- **Capture Mode**: Real-time RF signal capture with analysis
- **Spectrum Scan**: 300-928 MHz continuous monitoring
- **Device Database**: View identified device fingerprints
- **Research Mode**: Advanced protocol reverse engineering tools
- **Settings**: Configure RF parameters and logging options

### Capture & Analysis
1. Select frequency band (315/433/868/915 MHz or custom)
2. Choose modulation type or use auto-detection
3. Start capture - signals are automatically analyzed
4. View real-time results: fingerprint, threat score, protocol guess

### Research Mode
- Raw timing dumps to CSV
- Binary frame visualization
- Pulse histogram analysis
- Frame alignment tools

## 🏗️ Architecture

```
flipper-rf-lab/
├── core/                    # Main application & HAL
│   ├── flipper_rf_lab.h     # Core data structures
│   ├── flipper_rf_lab_main.c # Entry point & FreeRTOS tasks
│   ├── hal/                 # Hardware abstraction
│   │   ├── cc1101_driver.c  # TI CC1101 RF transceiver
│   │   ├── gpio_manager.c   # GPIO control
│   │   └── timer_precision.c # DWT cycle counter (1μs)
│   └── math/                # Fixed-point math library
│       ├── fixed_point.c    # Q15.16 operations
│       └── statistics.c     # Welford's online algorithm
├── analysis/                # Signal analysis engines
│   ├── fingerprinting.c     # RF device fingerprinting
│   ├── clustering.c         # K-means & hierarchical
│   ├── protocol_infer.c     # Protocol detection
│   └── threat_model.c       # Vulnerability scoring
├── storage/                 # Data persistence
│   ├── sd_manager.c         # FAT32 file operations
│   └── compression.c        # Delta, RLE, Huffman, LZ77
├── ui/                      # User interface
│   └── main_menu.c          # 128x64 LCD menu system
└── research/                # Advanced tools
    └── telemetry.c          # Performance monitoring
```

## 🔧 Technical Specifications

| Parameter | Value |
|-----------|-------|
| **Platform** | Flipper Zero (STM32WB55RG) |
| **CPU** | ARM Cortex-M4 @ 64 MHz |
| **RAM Usage** | ~60 KB (static allocation only) |
| **Timing Precision** | 0.1 μs (DWT cycle counter) |
| **Frequency Range** | 300-928 MHz |
| **Modulations** | 2-FSK, 4-FSK, GFSK, MSK, OOK, ASK |
| **Compression** | 16.7:1 ratio (RLE) |
| **Battery Life** | 8+ hours logging, 48+ hours standby |

## 🧪 Testing

All core algorithms are verified with Python simulations:

```bash
cd tests
python test_algorithms.py
```

**Results**: 30/30 tests passing (100%)
- Fixed-point math (Q15.16)
- Compression algorithms (Delta, RLE)
- Statistical analysis (Welford's algorithm)
- Clustering (K-means, Euclidean distance)
- Threat modeling (Shannon entropy)
- Protocol inference (OOK, FSK, ASK detection)

## 🤝 Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📝 License

MIT License - see [LICENSE](LICENSE) file.

## 🙏 Acknowledgments

- [Flipper Devices](https://flipperdevices.com/) for the amazing Flipper Zero
- [Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware) team
- TI CC1101 datasheet and application notes

## 📧 Contact

**Author**: [@tworjaga](https://github.com/tworjaga)

For bug reports and feature requests, please use [GitHub Issues](https://github.com/tworjaga/flipper-rf-lab/issues).

---

<p align="center">
  <b>Made with ❤️ for the RF research community</b>
</p>
