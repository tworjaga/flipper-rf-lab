# Installation Guide

## Table of Contents
- [Prerequisites](#prerequisites)
- [Method 1: Build from Source](#method-1-build-from-source-recommended)
- [Method 2: Pre-built Binary](#method-2-pre-built-binary)
- [Method 3: Flipper Lab / Mobile App](#method-3-flipper-lab--mobile-app)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Hardware
- **Flipper Zero** device (any revision)
- **MicroSD Card** (8GB+ recommended for logging)
- **USB-C Cable** for data transfer

### Optional Hardware
- External antenna (for improved range)
- CC1101 module (if not using built-in sub-GHz)

## Method 1: Build from Source (Recommended)

### Step 1: Install Prerequisites

#### Windows
```powershell
# Install WSL2 (Windows Subsystem for Linux)
wsl --install

# Or use MSYS2
# Download from https://www.msys2.org/
```

#### macOS
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install toolchain
brew install gcc-arm-embedded git python3
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y git python3 python3-pip gcc-arm-none-eabi binutils-arm-none-eabi
```

### Step 2: Clone Flipper Firmware
```bash
# Create workspace
mkdir -p ~/flipper-dev
cd ~/flipper-dev

# Clone official firmware
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
```

### Step 3: Add Flipper RF Lab
```bash
# Clone into applications_user directory
cd applications_user
git clone https://github.com/tworjaga/flipper-rf-lab.git
cd ..
```

### Step 4: Build
```bash
# Build the application
./fbt fap_flipper_rf_lab

# Output will be in:
# dist/f7-firmware-D/.extapps/flipper_rf_lab.fap
```

### Step 5: Flash to Flipper
```bash
# Connect Flipper via USB and flash
./fbt fap_flipper_rf_lab flash

# Or copy manually to SD card
cp dist/f7-firmware-D/.extapps/flipper_rf_lab.fap /path/to/flipper-sd/apps/
```

## Method 2: Pre-built Binary

### Download
1. Go to [Releases](https://github.com/tworjaga/flipper-rf-lab/releases)
2. Download `flipper_rf_lab.fap` for your firmware version

### Install
1. Connect Flipper Zero to computer via USB
2. Open [qFlipper](https://flipperzero.one/update) or [Flipper Lab](https://lab.flipper.net/)
3. Navigate to Apps → Install from file
4. Select downloaded `.fap` file

### Manual SD Card Install
1. Eject SD card from Flipper
2. Insert into computer
3. Copy `flipper_rf_lab.fap` to `/apps/` directory
4. Reinsert SD card into Flipper

## Method 3: Flipper Lab / Mobile App

### Using Flipper Mobile App
1. Install [Flipper Mobile App](https://flipperzero.one/app) (iOS/Android)
2. Connect to Flipper via Bluetooth
3. Go to Hub → Apps
4. Search for "Flipper RF Lab"
5. Tap Install

### Using Flipper Lab (Web)
1. Visit [lab.flipper.net](https://lab.flipper.net/)
2. Connect Flipper via USB
3. Go to Apps → Catalog
4. Search for "Flipper RF Lab"
5. Click Install

## Verification

### Check Installation
1. Power on Flipper Zero
2. Press **OK** to open main menu
3. Navigate to **Apps** → **GPIO** (or **Tools**)
4. Look for **Flipper RF Lab** icon

### First Run
1. Launch **Flipper RF Lab**
2. You should see the main menu with 4 options:
   - Capture Mode
   - Spectrum Scan
   - Device Database
   - Research Mode

3. Select **Settings** to verify:
   - Frequency: 433.92 MHz (default)
   - Modulation: Auto-detect
   - SD Card: Detected (if inserted)

## Troubleshooting

### "App not found" or missing icon
- **Solution**: Verify `.fap` file is in correct location (`/apps/` on SD card)
- Check firmware compatibility (see [Releases](https://github.com/tworjaga/flipper-rf-lab/releases))

### "SD Card Error"
- **Solution**: Format SD card as FAT32
- Ensure SD card is properly inserted
- Try different SD card (8GB+ recommended)

### Build errors
```bash
# Clean and rebuild
./fbt clean
./fbt fap_flipper_rf_lab

# Update submodules if needed
git submodule update --init --recursive
```

### Flipper not detected
- **Windows**: Install [Zadig](https://zadig.akeo.ie/) and install WinUSB driver
- **Linux**: Add udev rules (see [Flipper docs](https://docs.flipperzero.one/))
- **macOS**: No additional drivers needed

### App crashes on launch
- Check available RAM: Settings → System → Free
- Ensure firmware is up to date
- Try removing other apps to free memory

### RF not working
- Verify antenna is connected
- Check frequency band (315/433/868/915 MHz)
- Ensure no interference from other devices

## Next Steps

After successful installation:
1. Read the [User Guide](USER_GUIDE.md)
2. Try [Tutorial: First Capture](TUTORIAL_FIRST_CAPTURE.md)
3. Join [Discord community](https://discord.gg/flipper) for help

## Uninstallation

### Via qFlipper
1. Connect Flipper
2. Go to File Manager
3. Navigate to `/apps/`
4. Delete `flipper_rf_lab.fap`

### Via SD Card
1. Remove SD card
2. Delete `flipper_rf_lab.fap` from `/apps/`
3. Reinsert SD card

---

**Need help?** Open an [issue](https://github.com/tworjaga/flipper-rf-lab/issues) or join our [Discord](https://discord.gg/flipper).
