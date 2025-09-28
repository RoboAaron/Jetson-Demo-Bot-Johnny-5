# LDROBOT LiDAR Hardware Setup Guide

## Hardware Overview
- **Model**: LDROBOT STL-19P/D500 Kit
- **Interface**: USB-UART via CP2102 bridge
- **Power**: 5V via USB
- **Communication**: Serial (230400 baud, 8N1)

## Physical Setup

### 1. LiDAR Assembly
- Mount the LiDAR unit securely to your robot
- Ensure the scanning plane is horizontal
- Keep the unit level for accurate measurements

### 2. Power Connection
- Connect USB cable to power source (5V)
- **CRITICAL**: Ground the PWM pin for stable operation
- The LiDAR will not work properly without PWM pin grounded

### 3. USB Connection
- Connect to computer/robot controller
- Device should enumerate as `/dev/ttyUSB*` or `/dev/ldlidar`

## Software Setup

### 1. udev Rule Installation
```bash
# Copy udev rule
sudo cp configs/99-ldlidar.rules /etc/udev/rules.d/

# Reload rules
sudo udevadm control --reload-rules && sudo udevadm trigger

# Verify device appears
ls -la /dev/ldlidar
```

### 2. Port Permissions
```bash
# Set permissions
sudo chmod a+rw /dev/ldlidar

# Or add user to dialout group
sudo usermod -a -G dialout $USER
# (requires logout/login)
```

## Hardware Specifications

### LiDAR Specifications
- **Scan Rate**: 10 Hz
- **Angular Resolution**: ~0.7° (502 points per 360°)
- **Range**: 0.02m to 25m
- **Accuracy**: ±2cm
- **Field of View**: 360°

### Communication Protocol
- **Baud Rate**: 230400
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

### Power Requirements
- **Voltage**: 5V ±5%
- **Current**: ~200mA
- **Power**: ~1W

## Troubleshooting

### LiDAR Not Detected
1. Check USB connection
2. Verify udev rule is installed
3. Check device permissions
4. Try different USB port

### "Communication is abnormal"
1. Verify PWM pin is grounded
2. Check baud rate (must be 230400)
3. Ensure product_name is "LDLiDAR_LD19"
4. Check for port conflicts

### Poor Scan Quality
1. Ensure LiDAR is level
2. Check for obstructions
3. Verify power supply stability
4. Check for electromagnetic interference

### Low Scan Rate
1. Verify baud rate setting
2. Check for data corruption
3. Ensure exclusive port access
4. Check system performance

## Integration Notes

### Robot Integration
- Mount LiDAR at appropriate height for your application
- Consider mounting angle for optimal scanning
- Ensure stable mounting to prevent vibrations
- Plan cable routing to avoid interference

### Safety Considerations
- LiDAR emits laser light - avoid direct eye exposure
- Keep scanning area clear during operation
- Be aware of blind spots in robot design
- Consider environmental factors (dust, moisture)

## Performance Optimization

### For Maximum Performance
- Use dedicated USB port (not hub)
- Ensure stable power supply
- Minimize cable length
- Avoid electromagnetic interference

### For Power Efficiency
- Use appropriate scan rate for application
- Consider sleep modes if available
- Optimize processing algorithms
- Use efficient data structures
