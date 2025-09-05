# Ubuntu 22.04 Development Setup

*Guide to set up an Ubuntu 22.04 development environment for the Jetson Self-Balancing Robot*

## 1. Install System Packages

### Update and basic tools
```bash
sudo apt update
sudo apt install -y software-properties-common curl
```

### Python 3.11
```bash
sudo add-apt-repository ppa:deadsnakes/ppa -y
sudo apt update
sudo apt install -y python3.11 python3.11-venv python3.11-dev
```

### Git
```bash
sudo apt install -y git
```

### VS Code
```bash
sudo snap install code --classic
```

### Arduino IDE
```bash
sudo snap install arduino
```

### PuTTY for serial communication
```bash
sudo apt install -y putty
```

## 2. Install Python Packages
```bash
python3.11 -m pip install --user pyserial colorama python-can cantools
```

## 3. Manual Installations
1. Download Teensyduino from https://www.pjrc.com/teensy/td_download.html and install.
2. Download VESC Tool from https://vesc-project.com/vesc_tool and install.
3. Install VS Code extensions:
   - Arduino (Microsoft)
   - PlatformIO IDE
   - Python (Microsoft)

## 4. Verification
Run the provided test script to confirm installations:
```bash
python3 test_installations.py
```

## 5. Next Steps
- Connect the Teensy 4.1 and upload a test sketch.
- Connect the FSESC and test with VESC Tool.

---
*Ubuntu 22.04 equivalent of `setup_windows_dev.bat`*
*Last updated: 2025-02-14*
