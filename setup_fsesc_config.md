# FSESC6.7 Pro Configuration Setup

*Complete guide for configuring the Flipsky FSESC6.7 Pro motor controller for the Jetson Self-Balancing Robot*

## 1. Install VESC Tool

### Download VESC Tool
- Go to: https://vesc-project.com/vesc_tool
- Download version 6.02 or later
- Choose Windows/Linux/Mac version as needed

### Install VESC Tool
- **Windows**: Run the downloaded .exe installer
- **Linux**: Download AppImage or compile from source
- **Mac**: Download .dmg file and install

### Verify Installation
- Launch VESC Tool
- You should see the main interface with connection options

## 2. Connect FSESC

### Hardware Connection
- Connect FSESC to computer via USB cable
- Ensure FSESC is powered (connect battery or power supply)
- Check that power LED is on
- Verify USB connection is detected

### Software Connection
1. Open VESC Tool
2. Go to Connection → Connect
3. Select correct COM port (e.g., COM3, COM4)
4. Set baud rate to 115200 (default)
5. Click "Connect"

### Connection Troubleshooting
- Try different COM ports
- Check USB cable (use data cable)
- Restart VESC Tool
- Check Windows Device Manager for COM ports

## 3. Basic Configuration

### General Setup
1. **Connect to FSESC**
2. Go to **Setup → General**
3. Configure basic parameters:

#### Motor Configuration
- **Motor Type**: FOC (Field Oriented Control)
- **Motor Poles**: 14 (for hoverboard motors)
- **Motor KV**: 100-150 (typical for hoverboard motors)
- **Motor Resistance**: 0.1-0.3 ohms
- **Motor Inductance**: 0.1-0.5 mH

#### Current Limits
- **Max Battery Current**: 50A (FSESC6.7 Pro limit)
- **Max Motor Current**: 50A
- **Min Battery Current**: -50A (regenerative braking)
- **Min Motor Current**: -50A

#### Voltage Limits
- **Max Battery Voltage**: 42V (10S LiPo)
- **Min Battery Voltage**: 30V (10S LiPo cutoff)
- **Max Regen Voltage**: 45V

### CAN Bus Configuration
1. Go to **Setup → CAN Bus**
2. Configure CAN settings:
   - **CAN Baud Rate**: 500000 (500 kbps)
   - **CAN ID**: 1 (for first ESC), 2 (for second ESC)
   - **CAN Mode**: VESC
   - **CAN Status**: Enabled

## 4. Motor Calibration

### Motor Detection
1. Go to **Setup → Motor**
2. Click **"Detect Motor Parameters"**
3. Follow the on-screen instructions
4. **Important**: Ensure motor can rotate freely
5. Let the detection complete

### Manual Motor Parameters
If auto-detection fails, set manually:
- **Motor Resistance**: 0.15 ohms (typical)
- **Motor Inductance**: 0.2 mH (typical)
- **Motor Flux Linkage**: 0.01 Wb (typical)
- **Motor Poles**: 14

### Test Motor Rotation
1. Go to **Motor** tab
2. Set **Duty Cycle** to 5-10%
3. Click **"Start Motor"**
4. Verify motor rotates in correct direction
5. **Stop Motor** when done

## 5. Dual Motor Setup

### First ESC Configuration
1. Connect first FSESC
2. Set CAN ID to 1
3. Configure as above
4. Save configuration

### Second ESC Configuration
1. Connect second FSESC
2. Set CAN ID to 2
3. Configure with same parameters
4. Save configuration

### CAN Bus Testing
1. Connect both ESCs via CAN bus
2. Power on both ESCs
3. In VESC Tool, go to **CAN Bus** tab
4. You should see both ESCs listed
5. Test communication with each ESC

## 6. Balance Control Configuration

### PID Tuning
1. Go to **Setup → PID**
2. Configure balance control parameters:

#### Position PID
- **P**: 0.1-0.5 (start low)
- **I**: 0.0-0.1
- **D**: 0.0-0.1

#### Velocity PID
- **P**: 0.1-0.3
- **I**: 0.0-0.05
- **D**: 0.0-0.05

### Current Control
1. Go to **Setup → Current Control**
2. Set current control parameters:
- **Current Control Type**: FOC
- **Current Control Bandwidth**: 1000 Hz
- **Current Control Gain**: 0.1

## 7. Safety Configuration

### Emergency Stop
1. Go to **Setup → Safety**
2. Configure safety parameters:
- **Emergency Stop**: Enabled
- **Watchdog Timeout**: 1000 ms
- **Overcurrent Protection**: Enabled
- **Overtemperature Protection**: Enabled

### Brake Configuration
1. Go to **Setup → Brake**
2. Set brake parameters:
- **Brake Current**: 10A
- **Brake Time**: 1000 ms
- **Regen Brake**: Enabled

## 8. Data Logging and Monitoring

### Enable Logging
1. Go to **Data** tab
2. Select parameters to log:
   - Motor current
   - Battery voltage
   - Motor RPM
   - Temperature
   - CAN messages

### Real-time Monitoring
1. Go to **Realtime** tab
2. Monitor key parameters:
   - Motor current
   - Battery voltage
   - Motor RPM
   - Temperature

## 9. Testing and Validation

### Basic Function Test
1. Connect FSESC to motor
2. Power on system
3. Use VESC Tool to control motor
4. Test forward/reverse rotation
5. Test current limits
6. Test emergency stop

### CAN Communication Test
1. Connect both ESCs via CAN
2. Send commands via VESC Tool
3. Verify both motors respond
4. Test individual ESC control
5. Test synchronized control

### Safety Test
1. Test overcurrent protection
2. Test overtemperature protection
3. Test emergency stop
4. Test watchdog timeout
5. Test regenerative braking

## 10. Troubleshooting

### Common Issues

#### Connection Problems
- Check USB cable and port
- Verify power supply
- Try different COM port
- Restart VESC Tool

#### Motor Not Spinning
- Check motor connections
- Verify motor parameters
- Check current limits
- Test with low duty cycle

#### CAN Communication Issues
- Check CAN wiring
- Verify CAN IDs
- Check baud rate
- Test with CAN analyzer

#### Configuration Not Saving
- Ensure FSESC is connected
- Try different USB port
- Restart VESC Tool
- Check file permissions

### Getting Help
- VESC Forum: https://vesc-project.com/forum/
- Flipsky Support: Contact manufacturer
- VESC Documentation: https://vesc-project.com/documentation/

---

*This configuration guide ensures your FSESC6.7 Pro is properly set up for dual-motor balance control with CAN bus communication.*
