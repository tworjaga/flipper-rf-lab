# Contributing to Flipper RF Lab

Thank you for your interest in contributing to Flipper RF Lab! This document provides guidelines and instructions for contributing.

## 🎯 Ways to Contribute

- **Bug Reports**: Report issues via [GitHub Issues](https://github.com/tworjaga/flipper-rf-lab/issues)
- **Feature Requests**: Suggest new features or improvements
- **Code Contributions**: Submit pull requests with bug fixes or new features
- **Documentation**: Improve README, code comments, or add tutorials
- **Testing**: Help test on different Flipper Zero firmware versions

## 🚀 Development Setup

### Prerequisites

1. **Flipper Zero Firmware SDK**
   ```bash
   git clone https://github.com/flipperdevices/flipperzero-firmware.git
   cd flipperzero-firmware
   ```

2. **Toolchain**
   - Windows: Use [WSL2](https://docs.microsoft.com/en-us/windows/wsl/install) or [MSYS2](https://www.msys2.org/)
   - macOS: `brew install gcc-arm-embedded`
   - Linux: `sudo apt install gcc-arm-none-eabi`

3. **Python** (for testing)
   ```bash
   pip install -r tests/requirements.txt
   ```

### Project Structure

```
flipper-rf-lab/
├── core/           # Main application & hardware abstraction
├── analysis/       # Signal analysis engines
├── storage/        # SD card & compression
├── ui/             # User interface
├── research/       # Advanced tools
└── tests/          # Python algorithm verification
```

### Building

```bash
# Add to Flipper firmware
cp -r flipper-rf-lab flipperzero-firmware/applications_user/

# Build
cd flipperzero-firmware
./fbt fap_flipper_rf_lab

# Flash to device
./fbt fap_flipper_rf_lab flash
```

## 📝 Code Guidelines

### C Code Standards

1. **Memory Management**
   - NO `malloc/free` after initialization
   - Use static allocation with fixed-size pools
   - Maximum stack depth: 4KB per task

2. **Timing Constraints**
   - ISR latency: <10 microseconds
   - Signal sampling: 1 microsecond resolution minimum
   - Use DWT cycle counter for precision timing

3. **Math Operations**
   - NO floating-point in ISRs
   - Use Q15.16 fixed-point arithmetic
   - All real-time paths: fixed-point only

4. **Code Style**
   ```c
   // Function names: snake_case
   void cc1101_set_frequency(uint32_t freq_hz);
   
   // Struct names: PascalCase_t
   typedef struct {
       uint32_t drift_mean;
       uint16_t rise_time_avg;
   } RFFingerprint_t;
   
   // Constants: UPPER_CASE
   #define MAX_PULSE_COUNT 1000
   
   // Indentation: 4 spaces
   // Braces: K&R style
   ```

### Testing

All algorithms must have Python verification tests:

```python
# tests/test_new_feature.py
def test_new_algorithm():
    # Test implementation
    result = new_algorithm(input_data)
    assert result == expected_output
```

Run tests:
```bash
cd tests
python test_algorithms.py
```

## 🔧 Pull Request Process

1. **Fork** the repository
2. **Create a branch**: `git checkout -b feature/my-feature`
3. **Make changes** following code guidelines
4. **Test**: Run all tests, verify on actual hardware if possible
5. **Commit**: Use clear, descriptive commit messages
   ```
   feat: add new modulation detection algorithm
   
   - Implements FSK deviation analysis
   - Adds 5 new test cases
   - Verified on 433 MHz key fobs
   ```
6. **Push**: `git push origin feature/my-feature`
7. **Submit PR**: Fill out the PR template completely

## 🐛 Bug Reports

Include:
- Flipper Zero firmware version
- Steps to reproduce
- Expected vs actual behavior
- Serial logs (if applicable)
- Hardware configuration (external modules, etc.)

## 📋 Checklist for PRs

- [ ] Code follows style guidelines
- [ ] All tests pass (30/30)
- [ ] New features have tests
- [ ] Documentation updated
- [ ] No memory leaks (static allocation only)
- [ ] ISR timing verified (<10μs)
- [ ] Tested on actual Flipper Zero hardware

## 🏆 Recognition

Contributors will be:
- Listed in README.md
- Mentioned in release notes
- Added to CONTRIBUTORS file

## 💬 Questions?

- Open a [GitHub Discussion](https://github.com/tworjaga/flipper-rf-lab/discussions)
- Join the [Flipper Zero Discord](https://discord.gg/flipper)

## 📜 License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Thank you for helping make Flipper RF Lab better!** 🎉
