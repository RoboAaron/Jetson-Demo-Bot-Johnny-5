# Robot Tuning GUI

Real-time monitoring and parameter adjustment GUI for the self-balancing robot.

## Prerequisites

**IMPORTANT:** Before using this GUI, you must upload the robot firmware to your Teensy.

**Firmware (balance tuning):** `teensy_balance_cascaded/teensy_balance_cascaded.ino`  
**Alternative (logging-optimized):** `teensy_balance_logging_i2c_optimized/teensy_balance_logging_i2c_optimized.ino`

### Upload Instructions:
1. Open Arduino IDE
2. Open the firmware file (e.g. `teensy_balance_cascaded/teensy_balance_cascaded.ino`)
3. Select **Tools → Board → Teensy 4.1**
4. Select **Tools → USB Type → Serial**
5. Click **Upload** (or press Ctrl+U)
6. Wait for upload to complete

The firmware must be running on the Teensy for the GUI to receive data and control the robot.

## Features

- ✅ **Real-time IMU Data Display**: Roll, Pitch, Yaw with live updates
- ✅ **Communication Monitoring**: IMU and VESC communication rates and success percentages
- ✅ **Parameter Adjustment**: Adjust all PID gains and motor settings via GUI buttons
- ✅ **Real-time Plotting**: Live graphs of Roll, Pitch, and Current
- ✅ **Easy Device Connection**: Auto-detects serial devices with dropdown selection
- ✅ **Thread-safe**: Non-blocking serial communication using separate thread
- ✅ **Automatic Logging**: Saves all raw serial data to timestamped log files

## Installation

### Requirements

```bash
pip3 install pyserial matplotlib tkinter
```

On Ubuntu/Debian, you may need:
```bash
sudo apt-get install python3-tk
```

### Linux: Starting the GUI

1. **Open a terminal** and go to the repo root:
   ```bash
   cd /path/to/Jetson-Demo-Bot-Johnny-5
   ```

2. **Install Python dependencies** (one-time):
   ```bash
   pip3 install -r tuning_code/requirements.txt
   sudo apt-get install -y python3-tk
   ```

3. **Serial port access** (one-time, then log out and back in if you add yourself to `dialout`):
   ```bash
   # List ports: Teensy usually appears as /dev/ttyACM0
   ls -la /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
   # If you get "Permission denied" when connecting, add your user to dialout:
   sudo usermod -aG dialout $USER
   ```

4. **Start the GUI** (either from repo root or from `tuning_code`):
   ```bash
   cd tuning_code && python3 robot_tuning_gui.py
   ```
   Or use the helper script (from repo root or from `tuning_code`):
   ```bash
   cd tuning_code && ./run_gui.sh
   ```

## Usage

### Quick Start

```bash
cd tuning_code
python3 robot_tuning_gui.py
```

### What to do inside the GUI

1. **Connect to the robot**
   - Select the Teensy from the **Device** dropdown (e.g. `/dev/ttyACM0` on Linux). Click **Refresh** if the port is missing.
   - Click **Connect**. Status should turn green (**Connected**).

2. **Logging**
   - Use **Toggle Log** to start/stop saving a timestamped log file. Logs are written to `tuning_code/logs/robot_log_YYYYMMDD_HHMMSS.txt`.
   - **Open Logs** opens the logs folder. After a run, evaluate a log with:  
     `python3 tuning_code/log_evaluator.py tuning_code/logs/robot_log_YYYYMMDD_HHMMSS.txt` (from repo root).

3. **Balance tuning (single-loop then velocity)**
   - **Angle setpoint**: Use the **Angle Setpoint** ▼/▲ (or keys `z`/`Z`) to set target pitch (e.g. 0° for upright).
   - **Angle PID**: Use ▼/▲ on Angle Kp, Ki, Kd (or `p`/`P`, `i`/`I`, `d`/`D`) to tune stand/floor balance. Follow `teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md`.
   - **Max Current**: ▼/▲ (or `m`/`M`) to limit motor current.
   - **Velocity loop** (after angle loop is stable): Click **Toggle (v)** (or press `v`) to enable. Use **Vel Setpoint** ▼/▲ (or `6`/`V`) to change target velocity; **Stop (0)** sets velocity to 0.
   - **Velocity PID**: Vel Kp / Vel Ki ▼/▲ (or `w`/`W`, `e`/`E`) when tuning the velocity loop.

4. **Save to robot**
   - Press **k** (keyboard) to save current tuning to EEPROM so it persists across power cycles. (No GUI button; use keyboard.)

5. **Other**
   - **Show All Tuning Values** (or `x`): request full parameter list from the robot (printed to serial / GUI).
   - **Help**: show keyboard shortcuts.
   - Use the real-time plot and status to confirm data and behavior while tuning.

## GUI Layout

```
┌─────────────────────────────────────────────────────────┐
│  Device: [Dropdown] [Refresh] [Connect] Status: [OK]    │
├─────────────────────────────────────────────────────────┤
│  IMU Data & Monitoring    │  Tuning Parameters         │
│  Roll: 0.00°              │  Angle PID:                │
│  Pitch: 0.00°             │    Kp: 8.0 [▼] [▲]        │
│  Yaw: 0.00°               │    Ki: 0.2 [▼] [▲]         │
│  Balance: OK              │    Kd: 0.4 [▼] [▲]         │
│                           │  Velocity PID:             │
│  Communication Rates:     │    Kp: 0.4 [▼] [▲]        │
│  IMU: 400 Hz (99%)        │    Ki: 0.15 [▼] [▲]        │
│  VESC: 67 Hz (98%)        │    Kd: 0.15 [▼] [▲]       │
│                           │  Motor Settings:           │
│  [Real-time Plot]         │    Max Current: 6.0 [▼] [▲]│
│                           │    Setpoint: 0.0 [▼] [▲]   │
├─────────────────────────────────────────────────────────┤
│  [Show All] [Reset Position] [Help]                    │
└─────────────────────────────────────────────────────────┘
```

## Parameter Controls

### Angle PID (Outer Loop)
- **Kp**: Proportional gain (default: 8.0)
- **Ki**: Integral gain (default: 0.2)
- **Kd**: Derivative gain (default: 0.4)

### Velocity PID (Inner Loop)
- **Kp**: Proportional gain (default: 0.4)
- **Ki**: Integral gain (default: 0.15)
- **Kd**: Damping gain (default: 0.15)

### Motor Settings
- **Max Current**: Maximum motor current in Amps (default: 6.0)
- **Angle Setpoint**: Target balance angle in degrees (default: 0.0)

## Keyboard Shortcuts

All keyboard shortcuts from the serial monitor also work:
- `p/P` - Angle Kp decrease/increase
- `i/I` - Angle Ki decrease/increase
- `d/D` - Angle Kd decrease/increase
- `a/A` - Velocity Kp decrease/increase
- `b/B` - Velocity Ki decrease/increase
- `k/K` - Velocity Kd (damping) decrease/increase
- `m/M` - Max Current decrease/increase
- `z/Z` - Angle Setpoint decrease/increase
- `x` - Show all tuning values
- `r` - Reset position
- `h` - Show help

## Troubleshooting

### "No devices found"
- Make sure Teensy is plugged in via USB
- Close Arduino IDE Serial Monitor (it locks the port)
- Click "Refresh" to rescan devices
- Try: `ls -la /dev/ttyACM* /dev/ttyS*` to see available devices

### "Connection Error"
- Check that device is not in use by another program
- Verify baud rate is 2000000 (set in code)
- Try unplugging and replugging the Teensy

### "No Data" status
- Robot may not be sending data
- Check robot serial output in Arduino IDE
- Verify robot firmware is running
- Try clicking "Show All Tuning Values" to trigger response

### Plot not updating
- Ensure robot is sending data (check status)
- Data format must match: `R:0.00,P:0.00,Y:0.00,...`
- Check serial connection is active

## Architecture

- **Main Thread**: GUI (tkinter) - handles all UI updates
- **Serial Thread**: Reads serial data continuously (non-blocking)
- **Thread-safe**: Uses locks to protect shared data

## Data Format

The GUI expects serial data in this format:
```
R:0.00,P:0.00,Y:0.00,Pos:0.00,VelSet:0.00,VelAct:0.00,Curr:0.00,Bal:OK,Log:OFF
```

And stats in this format:
```
📊 I2C Stats: Success=1234 (99.5%), Fail=6, Total=1240
📊 VESC Stats: Success=567 (98.2%), Fail=10
```

## Logging

The GUI automatically saves all raw serial data to timestamped log files:

- **Location**: `tuning_code/logs/robot_log_YYYYMMDD_HHMMSS.txt`
- **Format**: Each line includes timestamp `[HH:MM:SS.mmm]` followed by raw serial data
- **Automatic**: Logging starts when you connect to the robot
- **Toggle**: Use "Toggle Log" button to enable/disable logging
- **View Logs**: Click "Open Log Folder" to view saved log files

### Log File Format

```
# Robot Serial Log - Started: 2025-01-02 14:30:15
# Device: /dev/ttyACM0
# Baud Rate: 2000000
# Raw serial data follows:
# ======================================================================

[14:30:15.123] R:0.00,P:0.00,Y:0.00,Pos:0.00,VelSet:0.00,VelAct:0.00,Curr:0.00,Bal:OK,Log:OFF
[14:30:15.173] 📊 I2C Stats: Success=1234 (99.5%), Fail=6, Total=1240
[14:30:15.223] R:0.12,P:-0.05,Y:114.26,Pos:0.00,VelSet:0.00,VelAct:0.00,Curr:0.00,Bal:OK,Log:OFF
...
```

## Log evaluation (log_evaluator.py)

Use the log evaluator to summarize tuning runs and get **clean** metrics for PID decisions. Logs often mix:

- **Wheel power on/off** (e.g. stand tests with motors disabled at times)
- **Manual disturbances** (holding the mast, pushing the robot)

The evaluator filters those out and reports both **all-row** (raw) and **clean** (disturbance-filtered) stats so gain changes are based on representative data.

### Run

**From repo root:**
```bash
python3 tuning_code/log_evaluator.py tuning_code/logs/robot_log_YYYYMMDD_HHMMSS.txt
```
Or by filename only (script looks in `tuning_code/logs/`):
```bash
python3 tuning_code/log_evaluator.py robot_log_20260218_225644.txt
```

**From `tuning_code/` directory:**
```bash
python3 log_evaluator.py logs/robot_log_YYYYMMDD_HHMMSS.txt
```

Options (tune for stand vs floor):

- `--disturbance-roll-rate N` — Roll rate (deg/s) above which a row is treated as disturbance (default: 30).
- `--motor-off-current N` — Current (A) below which motors are treated as off (default: 0.2).
- `--motor-off-error N` — |Error| (deg) below which row is motor-off when current is low (default: 1.0).

### Workflow

1. **Characterization pass** — Run with push/hold and wheel toggles; check disturbance counts and VEL SIGN MISMATCH.
2. **Clean tuning pass** — Wheels powered consistently, minimal manual input; use **clean** metrics for gain decisions.
3. If clean ratio is < 50%, retake logs before final PID changes.
4. Follow order: **signs → setpoint/current → angle loop → velocity loop** (see `teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md`).

## Notes

- GUI updates at 20 Hz (50ms intervals)
- Plot shows last 10 seconds of data
- Parameter values are estimated from serial responses
- For exact values, use "Show All Tuning Values" button
- Log files are saved automatically with timestamps

