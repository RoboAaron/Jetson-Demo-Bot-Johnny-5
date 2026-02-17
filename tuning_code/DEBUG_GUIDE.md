# GUI Debug Guide

## Debug Logging

The GUI now creates detailed debug logs to help diagnose serial communication issues.

### Log Location

Debug logs are saved in: `tuning_code/logs/gui_debug_YYYYMMDD_HHMMSS.log`

### What's Logged

- **Connection Events**: Every connect/disconnect attempt
- **Serial Operations**: All read/write operations
- **Errors**: All exceptions with full tracebacks
- **State Changes**: When connection state changes
- **Commands**: Every command sent to the robot
- **Read Loop Health**: Periodic status of the read loop

### How to Use

1. **Run the GUI**:
   ```bash
   cd tuning_code
   python3 robot_tuning_gui.py
   ```

2. **When issues occur**, check the debug log:
   ```bash
   ls -lt logs/gui_debug_*.log | head -1
   # View the most recent log
   tail -f logs/gui_debug_*.log  # Follow in real-time
   ```

3. **Look for**:
   - `[ERROR]` entries - These indicate problems
   - `[WARNING]` entries - These indicate potential issues
   - Connection/disconnection events
   - Serial read/write operations
   - Error tracebacks

### Common Issues in Logs

#### "Port is locked by another program"
- **Cause**: Another program (Arduino IDE, minicom, etc.) has the port open
- **Solution**: Close other serial programs, unplug/replug USB

#### "SerialException: device reports readiness to read but returned no data"
- **Cause**: Device disconnected or communication issue
- **Solution**: Check USB cable, restart Teensy

#### "Too many serial errors"
- **Cause**: Repeated I/O errors
- **Solution**: Check USB connection, verify device is powered

#### "Read loop exited" immediately after start
- **Cause**: Serial port closed or invalid
- **Solution**: Check device connection, verify port selection

### Debug Log Format

```
[HH:MM:SS.mmm] [LEVEL] Message
```

Example:
```
[14:23:45.123] [INFO] Attempting to connect to /dev/ttyACM0
[14:23:45.125] [INFO] Port test successful - port is available
[14:23:45.130] [INFO] Opening serial connection: /dev/ttyACM0 @ 2000000 baud
[14:23:45.135] [INFO] Serial port opened successfully
[14:23:45.140] [INFO] Read loop started
[14:23:45.200] [INFO] Reading 45 bytes from serial port
[14:23:50.000] [WARNING] No data received for 5.0 seconds
```

### Console Output

Errors and warnings are also printed to the console (terminal) in real-time for immediate feedback.

### Disabling Debug Logging

To disable debug logging, edit `robot_tuning_gui.py`:
```python
self.serial_reader = SerialReader(log_dir=log_dir, debug=False)
```

But **keep it enabled** when troubleshooting issues!

