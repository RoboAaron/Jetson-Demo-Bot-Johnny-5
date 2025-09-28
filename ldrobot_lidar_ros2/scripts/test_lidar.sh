#!/bin/bash

# LDROBOT LiDAR Test Script
# Tests LiDAR functionality and displays diagnostics

set -e

echo "Testing LDROBOT LiDAR..."

# Source ROS 2
source ~/ld_ws/install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedx_cpp

# Check if LiDAR is connected
if [ ! -e "/dev/ldlidar" ] && [ ! -e "/dev/serial/by-id/*CP210*" ]; then
    echo "ERROR: LiDAR not found. Please check:"
    echo "1. LiDAR is plugged in"
    echo "2. udev rule is installed: sudo cp configs/99-ldlidar.rules /etc/udev/rules.d/"
    echo "3. Run: sudo udevadm control --reload-rules && sudo udevadm trigger"
    exit 1
fi

# Find LiDAR port
PORT=$(readlink -f /dev/serial/by-id/*CP210* 2>/dev/null || echo /dev/ldlidar)
echo "Using LiDAR port: $PORT"

# Set permissions
sudo chmod a+rw "$PORT"

echo "Starting LiDAR test..."
echo "Press Ctrl+C to stop"

# Start LiDAR in background
ros2 run ldlidar_stl_ros2 ldlidar_stl_ros2_node --ros-args \
  -p product_name:=LDLiDAR_LD19 \
  -p port_name:="$PORT" \
  -p port_baudrate:=230400 \
  -p topic_name:=scan \
  -p frame_id:=laser &
LIDAR_PID=$!

# Wait for LiDAR to start
sleep 5

# Test topics
echo ""
echo "=== TOPIC TEST ==="
ros2 topic list | grep -E "(scan|tf)" || echo "No LiDAR topics found"

echo ""
echo "=== SCAN RATE TEST ==="
timeout 10 ros2 topic hz /scan || echo "No scan data received"

echo ""
echo "=== SCAN DATA TEST ==="
echo "Sample scan data:"
timeout 5 ros2 topic echo --no-arr /scan | head -n 10 || echo "No scan data received"

echo ""
echo "=== TF TREE TEST ==="
ros2 run tf2_tools view_frames
echo "TF tree saved to frames.pdf"

# Cleanup
echo ""
echo "Stopping LiDAR..."
kill $LIDAR_PID 2>/dev/null || true

echo "Test complete!"
