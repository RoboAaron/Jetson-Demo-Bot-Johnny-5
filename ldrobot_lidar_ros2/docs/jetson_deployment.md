# Jetson AGX Orin Deployment Guide

## Prerequisites
- Jetson AGX Orin with Ubuntu 22.04
- JetPack 5.1+ installed
- ROS 2 Humble installed
- LDROBOT LiDAR hardware

## Quick Deployment

### 1. Copy Configuration
```bash
# From development machine
scp -r ~/robotics_projects/ldrobot_lidar_ros2 user@jetson-ip:~/

# On Jetson
cd ~/ldrobot_lidar_ros2
./scripts/setup_lidar.sh
```

### 2. Verify Installation
```bash
# Test LiDAR
./scripts/test_lidar.sh

# Launch complete system
ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py
```

## Jetson-Specific Considerations

### Performance Optimization
```bash
# Set performance mode
sudo nvpmodel -m 0  # Maximum performance
sudo jetson_clocks  # Maximum clock speeds

# Set GPU memory
sudo sh -c 'echo 4 > /sys/kernel/debug/tegra_gpu/gpu_governor'
```

### Power Management
```bash
# For battery operation
sudo nvpmodel -m 2  # Balanced mode
sudo jetson_clocks --restore  # Restore default clocks
```

### USB Configuration
```bash
# Ensure USB 3.0 support
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666"' | \
sudo tee /etc/udev/rules.d/99-lidar-usb.rules
```

## ROS 2 Configuration

### Environment Setup
```bash
# Add to ~/.bashrc
export RMW_IMPLEMENTATION=rmw_cyclonedx_cpp
export AMENT_PYTHON_EXECUTABLE=/usr/bin/python3

# Source ROS 2
source /opt/ros/humble/setup.bash
source ~/ld_ws/install/setup.bash
```

### Launch Configuration
```yaml
# jetson_lidar.launch.py
- name: 'ldlidar_node'
  parameters:
    port_baudrate: 230400
    scan_topic: 'scan'
    frame_id: 'laser'
    # Jetson-specific optimizations
    transform_publish_period: 0.1
    map_update_interval: 0.2
```

## Integration Examples

### Navigation Stack
```python
# nav2_jetson.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription

def generate_launch_description():
    return LaunchDescription([
        # Include LiDAR
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('ldrobot_lidar_ros2'),
                    'launch',
                    'lidar_only.launch.py'
                ])
            ])
        ),
        # Navigation stack
        Node(
            package='nav2_bringup',
            executable='navigation_launch.py',
            parameters=[{
                'use_sim_time': False,
                'amcl': {
                    'laser_max_range': 25.0,
                    'laser_min_range': 0.02,
                    'laser_max_beams': 502
                }
            }]
        )
    ])
```

### SLAM Configuration
```yaml
# slam_jetson.yaml
slam_toolbox:
  ros__parameters:
    use_sim_time: false
    base_frame: base_link
    odom_frame: odom
    map_frame: map
    scan_topic: /scan
    mode: mapping
    # Jetson-optimized parameters
    transform_publish_period: 0.1
    map_update_interval: 0.2
    minimum_travel_distance: 0.2
    minimum_travel_heading: 0.2
    scan_queue_size: 50
```

## Performance Monitoring

### System Resources
```bash
# Monitor CPU/GPU usage
htop
tegrastats

# Monitor ROS 2 performance
ros2 topic hz /scan
ros2 topic bw /scan
```

### Memory Usage
```bash
# Check memory usage
free -h
nvidia-smi

# Monitor ROS 2 memory
ros2 node list
ros2 node info /ldlidar_node
```

## Troubleshooting

### Common Issues

#### Low Performance
- Check power mode: `sudo nvpmodel -q`
- Verify clock speeds: `sudo jetson_clocks --show`
- Monitor thermal throttling: `tegrastats`

#### USB Issues
- Check USB 3.0 connection
- Verify udev rules
- Check power supply

#### ROS 2 Issues
- Verify RMW implementation
- Check Python environment
- Monitor topic rates

### Debug Commands
```bash
# System info
uname -a
cat /etc/nv_tegra_release

# ROS 2 info
ros2 doctor
ros2 topic list
ros2 node list

# LiDAR specific
ros2 topic echo /scan --once
ros2 run tf2_tools view_frames
```

## Production Deployment

### Service Configuration
```bash
# Create systemd service
sudo tee /etc/systemd/system/lidar-slam.service > /dev/null << EOF
[Unit]
Description=LDROBOT LiDAR SLAM Service
After=network.target

[Service]
Type=simple
User=jetson
WorkingDirectory=/home/jetson/ld_ws
ExecStart=/bin/bash -c 'source install/setup.bash && ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py'
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Enable service
sudo systemctl enable lidar-slam.service
sudo systemctl start lidar-slam.service
```

### Logging Configuration
```bash
# Configure ROS 2 logging
export RCUTILS_LOGGING_USE_STDOUT=1
export RCUTILS_LOGGING_BUFFERED_STREAM=1
export RCUTILS_LOGGING_SEVERITY_THRESHOLD=INFO
```

## Maintenance

### Regular Checks
- Monitor scan quality
- Check for data drops
- Verify TF tree integrity
- Monitor system performance

### Updates
- Keep ROS 2 packages updated
- Monitor for driver updates
- Check for system updates
- Backup working configurations
