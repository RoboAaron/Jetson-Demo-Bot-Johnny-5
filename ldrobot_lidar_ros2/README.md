# LDROBOT LiDAR ROS 2 Integration Package

## Overview
Complete ROS 2 integration package for LDROBOT STL-19P/D500 LiDAR with SLAM capabilities. This package provides everything needed to integrate the LiDAR into robotics projects.

## Hardware Supported
- **LDROBOT STL-19P/D500 Kit**
- **USB-UART Bridge**: CP2102 (VID:PID 10c4:ea60)
- **Baud Rate**: 230400 (8N1)
- **Scan Rate**: 10 Hz, 502 ranges per scan
- **Range**: 0.02m to 25m

## Quick Start

### 1. Automated Setup
```bash
cd ~/robotics_projects/ldrobot_lidar_ros2
./scripts/setup_lidar.sh
```

### 2. Manual Setup
```bash
# Install dependencies
sudo apt install -y ros-humble-slam-toolbox python3-catkin-pkg-modules

# Clone driver
mkdir -p ~/ld_ws/src && cd ~/ld_ws/src
git clone https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git
cd ~/ld_ws && colcon build

# Setup udev rule
sudo cp configs/99-ldlidar.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 3. Launch LiDAR + SLAM
```bash
# Complete system
ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py

# Or individual components
ros2 launch ldlidar_stl_ros2 lidar_only.launch.py
ros2 launch ldlidar_stl_ros2 slam_only.launch.py
```

## Directory Structure
```
ldrobot_lidar_ros2/
├── README.md                    # This file
├── configs/                     # Configuration files
│   ├── 99-ldlidar.rules        # udev rule for device naming
│   ├── lidar_params.yaml       # LiDAR driver parameters
│   └── slam_params.yaml        # SLAM toolbox parameters
├── launch/                      # Launch files
│   ├── complete_lidar_slam.launch.py  # Full system
│   ├── lidar_only.launch.py    # LiDAR only
│   └── slam_only.launch.py     # SLAM only
├── scripts/                     # Utility scripts
│   ├── setup_lidar.sh          # Automated setup
│   ├── test_lidar.sh           # Test LiDAR functionality
│   └── export_config.sh        # Export working config
├── docs/                        # Documentation
│   ├── hardware_setup.md       # Hardware setup guide
│   ├── troubleshooting.md      # Common issues and solutions
│   └── jetson_deployment.md    # Jetson AGX Orin specific
└── examples/                    # Example integrations
    ├── navigation_example/      # Navigation stack integration
    ├── mapping_example/         # Mapping examples
    └── custom_robot/            # Custom robot integration
```

## Integration Examples

### Basic Robot Integration
```python
# In your robot's launch file
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # Include LiDAR launch
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('ldrobot_lidar_ros2'),
                    'launch',
                    'lidar_only.launch.py'
                ])
            ])
        ),
        # Your robot's other nodes...
    ])
```

### Navigation Stack Integration
```yaml
# nav2_params.yaml
amcl:
  ros__parameters:
    laser_model_type: "likelihood_field"
    laser_max_range: 25.0
    laser_min_range: 0.02
    laser_max_beams: 502
```

## Performance Specifications
- **Scan Rate**: 10 Hz
- **Angular Resolution**: ~0.7° (502 points/360°)
- **Range Accuracy**: ±2cm
- **Communication**: "ldlidar communication is normal"
- **TF Chain**: map → odom → base_link → laser

## Troubleshooting
See `docs/troubleshooting.md` for common issues and solutions.

## License
This package integrates with the official LDROBOT STL ROS 2 driver.
