# Frequently Asked Questions (FAQ)

## General Questions

### What is Flipper RF Lab?
Flipper RF Lab is a professional RF analysis and research platform for Flipper Zero. It transforms your device into a laboratory-grade RF forensics instrument with 15 advanced analysis features including device fingerprinting, protocol detection, and threat assessment.

### Is it free?
Yes! Flipper RF Lab is open source and free to use under the MIT License.

### Do I need special hardware?
No special hardware required - it works with stock Flipper Zero. However, an external antenna can improve range and sensitivity.

## Installation

### Where do I download it?
- **GitHub Releases**: [github.com/tworjaga/flipper-rf-lab/releases](https://github.com/tworjaga/flipper-rf-lab/releases)
- **Flipper Apps**: Available in the official Flipper app catalog
- **Build from source**: See [Installation Guide](INSTALLATION.md)

### What firmware version do I need?
Flipper RF Lab requires Flipper Zero firmware 0.93.0 or later. Check your version in Settings → System → Info.

### The app won't install / crashes
Common solutions:
1. Update Flipper firmware to latest version
2. Ensure SD card is inserted and formatted as FAT32
3. Check available storage space (need ~100KB)
4. Try installing via qFlipper instead of mobile app

### Build fails with "fbt: command not found"
Make sure you're in the flipperzero-firmware directory and have activated the environment:
```bash
source scripts/env.sh  # or scripts/env.ps1 on Windows
./fbt fap_flipper_rf_lab
```

## Usage

### How do I capture a signal?
1. Open Flipper RF Lab
2. Select **Capture Mode**
3. Choose frequency (433.92 MHz is common for key fobs)
4. Press **OK** to start
5. Press the button on your target device
6. View the analysis results

### What frequencies should I use?

| Device Type | Frequency |
|-------------|-----------|
| Car key fobs (US) | 315 MHz |
| Car key fobs (EU) | 433.92 MHz |
| Garage doors | 433.92 MHz |
| Weather stations | 433.92 or 868 MHz |
| Smart home | 868 or 915 MHz |

### Can I transmit/replay signals?
Yes, but **check local regulations first**! Some frequencies require licenses. The app includes:
- Signal replay with integrity checking
- Transmission quality analysis
- Power level adjustment (-20 to +10 dBm)

### Is it legal to use?
**Receiving**: Generally legal in most jurisdictions  
**Transmitting**: Check local laws - some frequencies require licenses

Common restrictions:
- 433.92 MHz: Usually license-free (ISM band)
- 315 MHz: Varies by country
- 868 MHz: License-free in Europe
- 915 MHz: License-free in Americas

### How accurate is the timing?
- **Precision**: 0.1 microseconds (DWT cycle counter)
- **Jitter**: <1 microsecond
- **Resolution**: 1 microsecond minimum

This is laboratory-grade precision suitable for academic research.

## Technical

### How much memory does it use?
- **RAM**: ~60 KB (static allocation only)
- **Flash**: ~80 KB for application
- **SD Card**: Variable (compressed captures ~5:1 ratio)

### Does it use floating-point?
**No** - All real-time calculations use Q15.16 fixed-point arithmetic for:
- Deterministic timing
- No FPU dependency
- Consistent performance

### What about battery life?
- **Active capture**: 6-8 hours
- **Passive scan**: 12-16 hours
- **Standby logging**: 48+ hours

Tips to extend battery:
- Reduce screen brightness
- Enable screensaver
- Use passive mode for monitoring

### Can I analyze encrypted signals?
The app captures and analyzes timing characteristics regardless of encryption. However:
- Encrypted payload content won't be readable
- Timing analysis still works (fingerprinting, clustering)
- Metadata is still available (RSSI, timing, etc.)

### What protocols are supported?
Auto-detection supports:
- **Modulations**: OOK, ASK, FSK, GFSK, MSK
- **Encodings**: NRZ, Manchester, Miller, PWM
- **Common chips**: PT2262, EV1527, HS1527, etc.

## Data & Storage

### Where is data saved?
```
/ext/apps_data/flipper_rf_lab/
├── captures/          # Raw and analyzed captures
├── fingerprints/      # Device database
├── logs/             # System logs and activity
└── exports/          # CSV/JSON exports
```

### How much can I store?
Depends on SD card size and compression:
- **Uncompressed**: ~1MB per 1000 frames
- **Compressed**: ~200KB per 1000 frames (5:1 ratio)
- **Rolling buffer**: Configurable (100MB - 1GB)

### Can I export data?
Yes! Export formats:
- **CSV**: Pulse timings for analysis in Excel/Python
- **JSON**: Structured data with metadata
- **Binary**: Raw capture for re-import

### Is my data private?
Yes - all data stays on your SD card. No cloud upload, no telemetry, completely offline.

## Troubleshooting

### "SD Card Error" message
1. Check SD card is properly inserted
2. Format as FAT32 (not exFAT or NTFS)
3. Try a different SD card (8GB+ recommended)
4. Check for write protection switch

### App freezes during capture
1. Reduce capture buffer size in settings
2. Lower sample rate
3. Ensure sufficient free RAM
4. Try disabling auto-save temporarily

### No signals detected
1. Verify correct frequency
2. Check antenna connection
3. Move closer to target device
4. Try different modulation settings
5. Use Spectrum Scan to find active frequencies

### Incorrect protocol detection
1. Manually select modulation type
2. Adjust data rate to match device
3. Check for interference (move to different location)
4. Use Research Mode for manual analysis

### Build warnings about "implicit declaration"
These are expected when building without the full Flipper SDK. The app uses SDK headers that aren't available in the host environment. The warnings don't affect functionality on actual hardware.

## Development

### Can I modify the code?
Yes! It's open source under MIT License. See [Contributing Guide](../CONTRIBUTING.md).

### How do I add a new feature?
1. Fork the repository
2. Create a feature branch
3. Implement with tests
4. Submit a pull request

See [Contributing Guide](../CONTRIBUTING.md) for detailed instructions.

### What coding standards?
- **Language**: C11 (no C++)
- **Memory**: Static allocation only (no malloc/free after init)
- **Timing**: <10μs ISR latency, fixed-point math only
- **Style**: 4-space indentation, snake_case functions

### How do I test changes?
```bash
cd tests
python test_algorithms.py
```

All 30 tests should pass before submitting PR.

## Advanced

### Can I use this for academic research?
Absolutely! The deterministic timing (<1μs jitter) and laboratory-grade precision make it suitable for:
- RF fingerprinting research
- Protocol reverse engineering
- Security vulnerability analysis
- Signal intelligence studies

Please cite: "Flipper RF Lab by tworjaga (github.com/tworjaga/flipper-rf-lab)"

### Is there an API for external tools?
Data can be exported via:
- **Serial**: Real-time data stream (CSV format)
- **SD Card**: Batch export (CSV/JSON)
- **File format**: Documented binary format

See [API Reference](API_REFERENCE.md) for details.

### Can I integrate with other software?
Exported data works with:
- **Universal Radio Hacker (URH)**
- **GNU Radio**
- **Python/NumPy/SciPy**
- **MATLAB**
- **Excel/LibreOffice**

### What about Bluetooth/WiFi analysis?
Currently focused on sub-GHz RF (300-928 MHz). Future versions may add:
- Bluetooth Low Energy (BLE) analysis
- WiFi spectrum monitoring
- NFC signal analysis

## Getting Help

### Where can I ask questions?
- **GitHub Discussions**: [github.com/tworjaga/flipper-rf-lab/discussions](https://github.com/tworjaga/flipper-rf-lab/discussions)
- **GitHub Issues**: For bugs and feature requests
- **Flipper Discord**: [discord.gg/flipper](https://discord.gg/flipper)

### I found a bug!
Please report it: [github.com/tworjaga/flipper-rf-lab/issues](https://github.com/tworjaga/flipper-rf-lab/issues)

Include:
- Flipper firmware version
- App version
- Steps to reproduce
- Expected vs actual behavior

### I have a feature request!
We welcome suggestions! Open a [feature request issue](https://github.com/tworjaga/flipper-rf-lab/issues/new?template=feature_request.md).

## Safety & Legal

### Is it safe for my Flipper?
Yes - the app respects hardware limits:
- No overclocking
- GPIO current limits enforced
- Temperature monitoring
- Safe RF power levels

### Can this be used for illegal purposes?
The app is designed for legitimate security research and education. Users are responsible for complying with local laws regarding:
- RF transmission regulations
- Privacy laws
- Computer fraud statutes

### Will this void my warranty?
No - using official Flipper apps doesn't void warranty. However, physical modifications (external amplifiers, etc.) might.

---

**Still have questions?**  
Open a [GitHub Discussion](https://github.com/tworjaga/flipper-rf-lab/discussions) or check the [User Guide](USER_GUIDE.md).
