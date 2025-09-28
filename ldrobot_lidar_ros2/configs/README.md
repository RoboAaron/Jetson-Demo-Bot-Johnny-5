# LDROBOT LiDAR ROS 2 Configuration Export

## Files Included:
- `lidar_params.yaml`: LiDAR driver parameters
- `slam_params.yaml`: SLAM toolbox parameters  
- `complete_lidar_slam.launch.py`: Complete launch file
- `setup_lidar.sh`: Automated setup script

## Quick Start:
1. Run `./setup_lidar.sh` on target system
2. Plug in LiDAR
3. Run `ros2 launch ldlidar_stl_ros2 complete_lidar_slam.launch.py`
4. Open RViz and configure displays

## Manual Launch (if needed):
```bash
# Terminal 1: LiDAR
ros2 run ldlidar_stl_ros2 ldlidar_stl_ros2_node --ros-args \
  -p product_name:=LDLiDAR_LD19 \
  -p port_name:=/dev/ldlidar \
  -p port_baudrate:=230400 \
  -p topic_name:=scan \
  -p frame_id:=laser

# Terminal 2: TF Chain
ros2 run tf2_ros static_transform_publisher 0 0 0 0 0 0 map odom &
ros2 run tf2_ros static_transform_publisher 0 0 0 0 0 0 odom base_link &
ros2 run tf2_ros static_transform_publisher 0 0 0 0 0 0 base_link laser &

# Terminal 3: SLAM
ros2 run slam_toolbox async_slam_toolbox_node --ros-args \
  -p use_sim_time:=false \
  -p base_frame:=base_link \
  -p odom_frame:=odom \
  -p map_frame:=map \
  -p scan_topic:=/scan \
  -p mode:=mapping

# Terminal 4: RViz
rviz2
```

## RViz Configuration:
- Fixed Frame: `base_link`
- Add LaserScan display (topic: `/scan`)
- Add Map display (topic: `/map`)
- Add TF display
