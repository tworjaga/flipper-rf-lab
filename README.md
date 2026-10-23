# Flipper RF Lab

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Flipper Zero](https://img.shields.io/badge/Platform-Flipper%20Zero-orange.svg)](https://flipperzero.one)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tworjaga/flipper-rf-lab)

An RF capture and analysis application for the Flipper Zero, built around an
external CC1101 transceiver. It records sub-GHz signals and runs on-device
analysis: device fingerprinting, protocol inference, clustering, entropy and
threat scoring, with fixed-point math and static memory allocation throughout.

This is research-oriented firmware. The analysis engines are implemented and
unit-tested; the on-device UI is currently a skeleton (see Status).

![flipper-rf-lab diagram](https://tworjaga.github.io/assets/diagram3.png)

| Component | State |
|-----------|-------|
| RF fingerprinting (timing drift, slopes, clock stability) | Implemented |
| Protocol inference (modulation/encoding/timing hypotheses) | Implemented |
| Threat modelling (entropy, static/CRC/rolling-code, replay) | Implemented |
| Clustering (k-means, silhouette) | Implemented |
| Hierarchical clustering / DTW | Partial (stubs) |
| Timing analysis (jitter, Allan variance) | Implemented |
| Signal compression (delta, RLE, LZ77) | Implemented |
| Huffman coding | Partial (encoder only) |
| Telemetry / performance counters | Implemented |
| Rolling long-term logging | Implemented |
| Fixed-point math toolkit (Q15.16) | Implemented |
| Spectrum sweep | Partial |
| Replay-integrity verification | Planned |
| On-device UI (per-mode screens) | Skeleton |

The main menu renders and the Back button exits cleanly. Per-mode screens,
hierarchical clustering, DTW, the Huffman decoder, replay integrity and some
export paths are stubbed or partial and are marked as such in the source.

## Hardware

The application drives an external CC1101 module over the Flipper's external SPI
bus; the built-in sub-GHz radio is not used. Wiring:

| CC1101 | Flipper pin |
|--------|-------------|
| CS     | PA4 (pin 4) |
| SCK    | PA5 (pin 5) |
| MOSI   | PA7 (pin 2) |
| MISO   | PA6 (pin 3) |
| GDO0   | PC3 (pin 7) |
| GDO2   | PC1 (pin 15) |

With no module attached the application still launches; capture simply returns
no data.

## Building

### With ufbt (recommended)

```bash
pip install ufbt
ufbt            # builds dist/flipper_rf_lab.fap
ufbt launch     # build, install and run on a connected Flipper
```

### Inside the firmware tree

```bash
git clone https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
git clone https://github.com/tworjaga/flipper-rf-lab.git applications_user/flipper-rf-lab
./fbt fap_flipper_rf_lab          # build
./fbt fap_flipper_rf_lab flash    # build and flash
```

### Prebuilt

Download the latest `.fap` from
[Releases](https://github.com/tworjaga/flipper-rf-lab/releases) and copy it to
`apps/RF/` on the Flipper's SD card.

## Usage

Main menu:

- Capture: real-time signal capture with analysis
- Spectrum Scan: 300-928 MHz sweep
- Fingerprint: device identification and database
- Threat Model: vulnerability assessment
- Research: protocol reverse-engineering tools
- Settings: RF parameters and logging options

Typical capture flow:

1. Select a frequency band (315 / 433 / 868 / 915 MHz, or custom).
2. Choose a modulation or use auto-detection.
3. Start capture; frames are analysed as they arrive.
4. Review results: fingerprint, threat score, protocol hypothesis.

## Architecture

```
flipper-rf-lab/
├── core/
│   ├── flipper_rf_lab.h        Core data structures and constants
│   ├── flipper_rf_lab_main.c   Entry point, worker threads, runtime
│   ├── circular_buffer.c       Lock-free ring buffer, static pool
│   ├── hal/
│   │   ├── cc1101_driver.c     CC1101 SPI driver
│   │   ├── gpio_manager.c      GPIO configuration
│   │   └── timer_precision.c   DWT cycle-counter timing
│   └── math/
│       ├── fixed_point.c       Q15.16 fixed-point operations
│       └── statistics.c        Welford, histograms, regression
├── analysis/
│   ├── fingerprinting.c        RF device fingerprinting
│   ├── clustering.c            K-means and distance metrics
│   ├── protocol_infer.c        Protocol hypothesis generation
│   └── threat_model.c          Entropy and vulnerability scoring
├── storage/
│   ├── sd_manager.c            SD sessions, logging, export
│   └── compression.c           Delta, RLE, LZ77, Huffman
├── ui/
│   └── main_menu.c             128x64 menu
├── research/
│   └── telemetry.c             Performance monitoring
└── tests/                      Host unit tests (see Testing)
```

## Technical specifications

| Parameter | Value |
|-----------|-------|
| Target | Flipper Zero (STM32WB55RG, Cortex-M4 @ 64 MHz) |
| Static RAM | ~80 KB (.bss); no heap allocation at runtime |
| Timing | DWT cycle counter, ~1 µs resolution |
| Frequency range | 300-928 MHz (CC1101 dependent) |
| Modulations | 2-FSK, 4-FSK, GFSK, MSK, OOK, ASK |
| Compression | Delta / RLE / LZ77 (ratio is data-dependent) |

## Testing

Two suites are provided.

Real-source C tests link against the actual production code
(`core/math/fixed_point.c`, `core/math/statistics.c`, `storage/compression.c`)
through a small SDK shim, so they exercise the shipping implementation rather
than a copy. They include regressions for specific fixed bugs (delta-codec
escape-byte collisions, pulse-sequence framing, fixed-point overflow).

```bash
tests\run_host_tests.cmd        # MSVC, from the repo root

# or with gcc:
gcc -I tests/hostshim -I core -o test_real tests/test_real.c \
    core/math/fixed_point.c core/math/statistics.c storage/compression.c
./test_real
```

A Python suite provides a quick, compiler-free behavioural check. Note that it
exercises reference reimplementations of the algorithms, not the C sources; use
the C tests above to validate shipping code.

```bash
python tests/test_algorithms.py
```

## License

MIT. See [LICENSE](LICENSE).

## Acknowledgments

- Flipper Devices and the Flipper Zero firmware team
- Texas Instruments CC1101 datasheet and application notes
