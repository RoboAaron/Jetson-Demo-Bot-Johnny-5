# Quick Reference Guide

*Essential commands and configurations for the Jetson Self-Balancing Robot*

## Essential Commands

### System Management
```bash
# Check Jetson status and power
sudo jetson_clocks --show
sudo tegrastats

# Monitor system resources
htop
nvidia-smi

# Check USB devices (for OAK-D camera)
lsusb
lsusb -t
```

### ROS 2 Commands
```bash
# Source environment
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# Check ROS 2 status
ros2 node list
ros2 topic list
ros2 topic echo /topic_name

# Launch robot
ros2 launch robot_bringup robot.launch.py

# Debug tools
rqt
rviz2
ros2 run rqt_console rqt_console
```

### Camera Testing
```bash
# Test OAK-D camera
python3 -c "import depthai as dai; print('DepthAI version:', dai.__version__)"
python3 ~/depthai/depthai_demo.py

# ROS 2 camera launch
ros2 launch depthai_ros oak_d_pro_launch.py
```

### Audio Testing
```bash
# Test microphone
arecord -l
arecord -f cd -d 5 test.wav

# Test Whisper
whisper test.wav --model base
```

### CAN Bus Testing
```bash
# Check CAN interface
ip link show can0

# Send test message
cansend can0 123#DEADBEEF

# Monitor CAN traffic
candump can0
```

### Teensy Development
```bash
# Upload firmware (Arduino IDE)
# Tools -> Board -> Teensy 4.1
# Tools -> Port -> /dev/ttyACM0
# Click Upload

# Serial monitor
screen /dev/ttyACM0 115200
```

## Configuration Files

### Network Configuration
```bash
# WiFi setup
sudo nmcli dev wifi list
sudo nmcli dev wifi connect "SSID" password "password"

# Static IP (if needed)
sudo nano /etc/netplan/01-network-manager-all.yaml
```

### ROS 2 Discovery
```bash
# Set domain ID
export ROS_DOMAIN_ID=0

# Check discovery
ros2 daemon status
ros2 node list
```

### Camera Configuration
```yaml
# ~/ros2_ws/src/depthai-ros/depthai_ros/launch/oak_d_pro_launch.py
# Modify resolution, FPS, and other parameters
```

## Troubleshooting

### Common Issues

#### Balance Problems
```bash
# Check IMU data
ros2 topic echo /imu/data

# Verify control loop frequency
ros2 topic hz /cmd_vel
```

#### Camera Issues
```bash
# Check USB connection
lsusb | grep Luxonis

# Reset camera
sudo usb_modeswitch -v 0x03e7 -p 0x2485 -R
```

#### Power Issues
```bash
# Check battery voltage
ros2 topic echo /battery_voltage

# Monitor power consumption
sudo tegrastats
```

#### ROS 2 Communication
```bash
# Restart ROS 2 daemon
ros2 daemon stop
ros2 daemon start

# Check network connectivity
ping <robot_ip>
```

## Safety Commands

### Emergency Stop
```bash
# Software emergency stop
ros2 topic pub /emergency_stop std_msgs/msg/Bool "data: true"

# Hardware emergency stop
# Physical battery switch on robot
```

### System Shutdown
```bash
# Graceful shutdown
ros2 lifecycle set /robot_node shutdown

# Force shutdown
sudo shutdown -h now
```

## Development Workflow

### Daily Startup
```bash
# 1. Power on robot
# 2. Connect to WiFi
# 3. SSH to robot
ssh nvidia@<robot_ip>

# 4. Source environment
source ~/ros2_ws/install/setup.bash

# 5. Launch robot
ros2 launch robot_bringup robot.launch.py
```

### Code Development
```bash
# Create new package
cd ~/ros2_ws/src
ros2 pkg create --build-type ament_python robot_package

# Build workspace
cd ~/ros2_ws
colcon build --symlink-install

# Test package
ros2 run robot_package node_name
```

### Firmware Updates
```bash
# Teensy firmware
# 1. Open Arduino IDE
# 2. Load sketch
# 3. Select Teensy 4.1 board
# 4. Upload

# Jetson software
cd ~/ros2_ws
git pull
colcon build --symlink-install
```

## Performance Monitoring

### System Resources
```bash
# CPU and memory
htop

# GPU usage
nvidia-smi

# Disk usage
df -h

# Network usage
nethogs
```

### ROS 2 Performance
```bash
# Topic frequency
ros2 topic hz /topic_name

# Node CPU usage
ros2 run resource_monitor resource_monitor

# Message latency
ros2 topic echo /topic_name --once
```

## Backup and Recovery

### System Backup
```bash
# Create system image
sudo dd if=/dev/mmcblk0 of=/path/to/backup.img bs=4M

# Backup workspace
tar -czf robot_workspace_backup.tar.gz ~/ros2_ws
```

### Configuration Backup
```bash
# Backup configurations
cp -r ~/.bashrc ~/robot_configs/
cp -r /etc/netplan/ ~/robot_configs/
cp -r ~/ros2_ws/src/robot_bringup/launch/ ~/robot_configs/
```

---

*Keep this reference handy during development and operation of the robot.*

*Last updated: 2025-01-27*
