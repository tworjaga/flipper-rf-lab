# Flipper RF Lab Documentation

Welcome to the Flipper RF Lab documentation! This directory contains comprehensive guides for users and developers.

## 📚 Documentation Index

### For Users
- **[Installation Guide](INSTALLATION.md)** - Step-by-step installation instructions
- **[User Guide](USER_GUIDE.md)** - Complete usage manual with all features
- **[FAQ](FAQ.md)** - Frequently asked questions and troubleshooting

### For Developers
- **[API Reference](API_REFERENCE.md)** - Complete API documentation
- **[Contributing Guide](../CONTRIBUTING.md)** - How to contribute to the project
- **[Architecture Overview](ARCHITECTURE.md)** - System design and components

### Reference
- **[Changelog](../CHANGELOG.md)** - Version history and changes
- **[License](../LICENSE)** - MIT License terms

## 🚀 Quick Links

| I want to... | Go to... |
|-------------|----------|
| Install the app | [Installation Guide](INSTALLATION.md) |
| Learn how to use it | [User Guide](USER_GUIDE.md) |
| Understand the code | [API Reference](API_REFERENCE.md) |
| Report a bug | [GitHub Issues](https://github.com/tworjaga/flipper-rf-lab/issues) |
| Contribute code | [Contributing Guide](../CONTRIBUTING.md) |

## 🎯 Feature Overview

Flipper RF Lab provides 15 professional RF analysis features:

1. **RF Fingerprinting** - Identify devices by unique RF characteristics
2. **Adaptive Signal Modeling** - Automatic protocol detection
3. **Cross-Session Correlation** - Compare signals across captures
4. **Behavioral Profiling** - Long-term device analysis
5. **Timing Stability Analysis** - Laboratory-grade measurements
6. **Advanced Clustering** - Signal grouping algorithms
7. **Threat Modeling** - Security vulnerability assessment
8. **Real-Time Activity Map** - Spectrum monitoring
9. **Signal Replay Integrity** - Verify transmission fidelity
10. **Modular Research Mode** - Reverse engineering tools
11. **Internal Telemetry** - Performance monitoring
12. **Deterministic Execution** - <1μs timing precision
13. **Signal Compression** - 5:1+ storage efficiency
14. **Long-Term Logging** - Unattended monitoring
15. **Embedded Math Toolkit** - Fixed-point operations

## 📖 Reading Order

### New Users
1. [Installation Guide](INSTALLATION.md)
2. [User Guide](USER_GUIDE.md) - Getting Started section
3. [User Guide](USER_GUIDE.md) - Capture Mode section

### Developers
1. [Contributing Guide](../CONTRIBUTING.md)
2. [Architecture Overview](ARCHITECTURE.md)
3. [API Reference](API_REFERENCE.md)

### Researchers
1. [User Guide](USER_GUIDE.md) - Research Mode section
2. [API Reference](API_REFERENCE.md) - Analysis Engines
3. [Architecture Overview](ARCHITECTURE.md) - Signal Processing

## 🔧 Technical Specifications

- **Platform**: Flipper Zero (STM32WB55RG)
- **Language**: C11 (no C++)
- **Memory**: Static allocation only, ~60KB RAM usage
- **Timing**: 0.1μs precision (DWT cycle counter)
- **Frequency**: 300-928 MHz coverage
- **UI**: 128×64 monochrome LCD, 30fps

## 🤝 Support

- **GitHub Issues**: [github.com/tworjaga/flipper-rf-lab/issues](https://github.com/tworjaga/flipper-rf-lab/issues)
- **Discussions**: [GitHub Discussions](https://github.com/tworjaga/flipper-rf-lab/discussions)
- **Discord**: [Flipper Zero Community](https://discord.gg/flipper)

## 📝 Contributing to Documentation

We welcome documentation improvements! Please see the [Contributing Guide](../CONTRIBUTING.md) for guidelines.

### Documentation Style Guide
- Use clear, concise language
- Include code examples where helpful
- Add screenshots for UI features
- Keep line length under 100 characters
- Use relative links for internal references

---

**Last Updated**: 2024-01-XX  
**Documentation Version**: 1.0.0
