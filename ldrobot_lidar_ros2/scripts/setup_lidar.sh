#!/bin/bash

# LDROBOT LiDAR ROS 2 Setup Script
# Automated setup for Ubuntu 22.04 + ROS 2 Humble

set -e

echo "Setting up LDROBOT LiDAR ROS 2..."

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Environment setup
export AMENT_PYTHON_EXECUTABLE=/usr/bin/python3
export RMW_IMPLEMENTATION=rmw_cyclonedx_cpp

echo "Installing system dependencies..."
sudo apt update
sudo apt install -y python3-rosdep ca-certificates curl
sudo apt install -y python3-catkin-pkg-modules
sudo apt install -y ros-humble-slam-toolbox

# Fix rosdep
echo "Configuring rosdep..."
sudo rm -f /etc/ros/rosdep/sources.list.d/20-default.list
sudo rosdep init || true
rosdep update

# Clone and build LiDAR driver
echo "Installing LiDAR driver..."
mkdir -p ~/ld_ws/src && cd ~/ld_ws/src
if [ ! -d "ldlidar_stl_ros2" ]; then
    git clone https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git
fi
cd ~/ld_ws
rm -rf build/ install/ log/
colcon build
source install/setup.bash

# Setup udev rule
echo "Setting up udev rule..."
sudo cp "$PROJECT_DIR/configs/99-ldlidar.rules" /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger

# Copy launch files
echo "Installing launch files..."
mkdir -p ~/ld_ws/src/ldlidar_stl_ros2/launch
cp "$PROJECT_DIR/launch/"*.py ~/ld_ws/src/ldlidar_stl_ros2/launch/
cd ~/ld_ws && colcon build

echo ""
echo "Setup complete!"
echo ""
echo "To test the LiDAR:"
echo "1. Plug in the LiDAR"
echo "2. Run: ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py"
echo "3. Open RViz and configure displays"
echo ""
echo "For manual testing:"
echo "ros2 run ldlidar_stl_ros2 ldlidar_stl_ros2_node --ros-args \\"
echo "  -p product_name:=LDLiDAR_LD19 \\"
echo "  -p port_name:=/dev/ldlidar \\"
echo "  -p port_baudrate:=230400"
