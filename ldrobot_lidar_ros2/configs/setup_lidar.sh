#!/bin/bash

# LDROBOT LiDAR ROS 2 Quick Setup Script
# Run this on the target system (e.g., Jetson AGX Orin)

echo "Setting up LDROBOT LiDAR ROS 2..."

# Environment setup
export AMENT_PYTHON_EXECUTABLE=/usr/bin/python3
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

# Install dependencies
sudo apt update
sudo apt install -y python3-rosdep ca-certificates curl
sudo apt install -y python3-catkin-pkg-modules
sudo apt install -y ros-humble-slam-toolbox

# Fix rosdep
sudo rm -f /etc/ros/rosdep/sources.list.d/20-default.list
sudo rosdep init
rosdep update

# Clone and build
mkdir -p ~/ld_ws/src && cd ~/ld_ws/src
git clone https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git
cd ~/ld_ws
rm -rf build/ install/ log/
colcon build
source install/setup.bash

# Setup udev rule
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="ldlidar", MODE="0666"' | \
sudo tee /etc/udev/rules.d/99-ldlidar.rules
sudo udevadm control --reload-rules && sudo udevadm trigger

echo "Setup complete! Plug in LiDAR and run:"
echo "ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py"
