# Quick Start Guide

## Prerequisites

**Upload Firmware First!**

The robot must be running this firmware:
- **File:** `teensy_balance_logging_i2c_optimized/teensy_balance_logging_i2c_optimized.ino`
- **Board:** Teensy 4.1
- **Upload via Arduino IDE** before using the GUI

## Installation (One-time)

```bash
cd tuning_code
pip3 install -r requirements.txt

# On Ubuntu/Debian, also install tkinter:
sudo apt-get install python3-tk
```

## Running the GUI

### Option 1: Direct Python
```bash
cd tuning_code
python3 robot_tuning_gui.py
```

### Option 2: Quick Start Script
```bash
cd tuning_code
./run_gui.sh
```

## First Use

1. **Connect Robot**:
   - Plug in Teensy via USB
   - Close Arduino IDE Serial Monitor (if open)
   - In GUI: Select device from dropdown → Click "Connect"
   - Status should turn green: "Connected"

2. **Verify Data**:
   - IMU values should update (Roll, Pitch, Yaw)
   - Communication rates should show (IMU Hz, VESC Hz)
   - Plot should start showing data

3. **Adjust Parameters**:
   - Use ▼/▲ buttons to change values
   - Changes are sent immediately
   - Click "Show All Tuning Values" to see current state

## Troubleshooting

**"No devices found"**
- Close Arduino IDE Serial Monitor
- Unplug/replug Teensy
- Click "Refresh" button

**"Connection Error"**
- Check USB cable
- Try different USB port
- Verify Teensy is powered

**GUI shows "No Data"**
- Check robot is running firmware
- Verify serial output in Arduino IDE
- Try clicking "Show All Tuning Values"

## Tips

- Keep GUI open while tuning - see changes in real-time
- Watch the plot to see how parameters affect behavior
- Communication rates should be >95% for reliable operation
- Use "Show All Tuning Values" to verify current settings

