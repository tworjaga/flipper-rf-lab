# Changelog

All notable changes to Flipper RF Lab will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2024-01-XX

### Added
- Initial release with 15 core features
- RF Fingerprinting Engine - Device-level identification via timing analysis
- Adaptive Signal Modeling - Automatic protocol detection
- Cross-Session Correlation - Compare signals across captures
- Behavioral Profiling - Long-term device analysis
- Timing Stability Analysis - Laboratory-grade oscillator characterization
- Advanced Clustering - K-means and hierarchical signal grouping
- Threat Modeling - Vulnerability scoring and risk assessment
- Real-Time Activity Map - 300-928 MHz spectrum monitoring
- Signal Replay Integrity - Verify signal regeneration fidelity
- Modular Research Mode - Protocol reverse engineering tools
- Internal Telemetry - Performance monitoring
- Deterministic Execution - <1μs timing jitter
- Signal Compression - 5:1+ compression ratio
- Long-Term Logging - Unattended background monitoring
- Embedded Math Toolkit - Q15.16 fixed-point operations

### Technical
- Pure C implementation (C11 standard)
- Zero dynamic memory allocation after initialization
- Static allocation with fixed-size pools
- DMA usage for SPI transfers
- Circular buffers for streaming data
- DWT cycle counter for 1μs precision timing
- FreeRTOS task management
- FAT32 filesystem support

### Hardware Support
- TI CC1101 RF transceiver
- 315/433/868/915 MHz frequency bands
- 2-FSK, 4-FSK, GFSK, MSK, OOK, ASK modulations
- 0.6 to 500 kBaud data rates

### Testing
- 30/30 algorithm verification tests passing
- Fixed-point math validation
- Compression algorithm verification
- Statistical analysis testing
- Clustering algorithm validation
- Threat model entropy analysis
- Protocol inference testing

## [Unreleased]

### Planned
- Bluetooth Low Energy (BLE) analysis support
- NFC signal analysis integration
- Infrared protocol analysis
- Machine learning-based classification
- Web-based analysis dashboard
- Signal database sharing
- Automated vulnerability reporting

---

**Legend:**
- `Added` - New features
- `Changed` - Changes to existing functionality
- `Deprecated` - Soon-to-be removed features
- `Removed` - Removed features
- `Fixed` - Bug fixes
- `Security` - Security improvements
