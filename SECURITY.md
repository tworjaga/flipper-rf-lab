# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

We take security seriously. If you discover a security vulnerability, please report it responsibly.

### How to Report

**Please DO NOT** open a public GitHub issue for security vulnerabilities.

Instead, report privately via:
- GitHub Security Advisories: [github.com/tworjaga/flipper-rf-lab/security/advisories](https://github.com/tworjaga/flipper-rf-lab/security/advisories)
- Or email: security@tworjaga.dev (if available)

### What to Include

When reporting a vulnerability, please include:

1. **Description**: Clear description of the vulnerability
2. **Impact**: What could an attacker achieve?
3. **Steps to Reproduce**: Detailed instructions
4. **Affected Versions**: Which versions are vulnerable?
5. **Mitigation**: Any workarounds you've identified
6. **Your Contact**: How to reach you for follow-up

### Response Timeline

| Phase | Timeframe |
|-------|-----------|
| Initial Response | Within 48 hours |
| Assessment | Within 1 week |
| Fix Development | Depends on severity |
| Public Disclosure | After fix is available |

### Severity Classification

- **Critical**: Remote code execution, data breach
- **High**: Local privilege escalation, significant data exposure
- **Medium**: Denial of service, information disclosure
- **Low**: Minor issues, hard to exploit

## Security Considerations

### RF Transmission Safety

This application can transmit RF signals. Users must:

1. **Comply with local regulations** regarding RF transmissions
2. **Use appropriate power levels** for the frequency band
3. **Avoid interfering** with emergency services or critical infrastructure
4. **Obtain necessary licenses** if required in your jurisdiction

### Data Privacy

- All captured data stays on the SD card
- No cloud connectivity or telemetry
- No automatic data transmission
- Users have full control over their data

### Hardware Safety

The application respects hardware limits:
- GPIO current limits enforced (max 20mA per pin)
- No overclocking or voltage manipulation
- Temperature monitoring
- Safe RF power levels (within CC1101 specifications)

### Known Limitations

1. **Physical Access**: Anyone with physical access to the device can access captured data
2. **SD Card**: Data on SD card is unencrypted - protect physical access
3. **RF Eavesdropping**: Captured signals could be replayed if not properly secured

## Best Practices for Users

### Secure Usage

1. **Physical Security**: Keep your Flipper Zero physically secure
2. **SD Card Encryption**: Consider encrypting sensitive captures
3. **Responsible Disclosure**: Report vulnerabilities you find in other devices responsibly
4. **Legal Compliance**: Always follow local laws regarding RF analysis

### For Security Researchers

This tool is designed for legitimate security research:

- **Device Testing**: Test your own devices
- **Authorized Testing**: Only test devices you own or have permission to test
- **Responsible Disclosure**: Report findings to manufacturers
- **Education**: Use for learning RF security concepts

## Security Features

The application includes security analysis features:

- **Threat Modeling**: Identifies vulnerable protocols
- **Entropy Analysis**: Detects weak encryption/static codes
- **Fingerprinting**: Identifies specific devices
- **Vulnerability Scoring**: Rates security risks

These features are for defensive purposes - identifying weaknesses in your own devices.

## Third-Party Dependencies

This application uses:
- Flipper Zero Firmware SDK (official)
- FreeRTOS (embedded in SDK)
- FATFS (embedded in SDK)

All dependencies are part of the official Flipper ecosystem.

## Updates and Patches

Security updates will be:
- Released as soon as possible
- Documented in [CHANGELOG.md](CHANGELOG.md)
- Announced via GitHub Releases
- Backported to supported versions when feasible

## Acknowledgments

We thank security researchers who responsibly disclose vulnerabilities.

**Hall of Fame** (researchers who reported valid security issues):
- *None yet - be the first!*

## Contact

For security-related questions:
- Security Advisories: [GitHub Security](https://github.com/tworjaga/flipper-rf-lab/security)
- General Questions: [GitHub Discussions](https://github.com/tworjaga/flipper-rf-lab/discussions)

---

**Last Updated**: 2024-01-XX  
**Policy Version**: 1.0
