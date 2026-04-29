# Demo Runbook: C3 — SLAM (LiDAR Mapping)

**PBI:** PBI-16 (Wave 2A)  
**Hardware:** Teensy + LDROBOT STL-19P LiDAR  
**Prerequisite tests:** A1 all PASS, A2 all PASS, B1 PASS, C1 PASS

---

## Prerequisites

- [ ] Balance demo (C1) passes — robot balances reliably
- [ ] LiDAR detected and publishing /scan (A2.1–A2.5)
- [ ] Bridge + LiDAR joint test passes (B1)
- [ ] `slam_toolbox` installed (`ros2 pkg list | grep slam_toolbox`)
- [ ] base_link → laser static TF configured
- [ ] Robot on floor with room to drive around

## Steps

### 1. Launch bridge + LiDAR + static TF

```bash
# Terminal 1 — use the sensor_test launch for a clean minimal start
source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash
ros2 launch johnny5_bringup sensor_test.launch.py \
    enable_bridge:=true \
    enable_lidar:=true \
    enable_static_tf:=true \
    laser_x:=0.0 laser_y:=0.0 laser_z:=0.15
```

Wait for both bridge and LiDAR to report connected.

### 2. Verify prerequisites

```bash
# Terminal 2
ros2 topic hz /scan     # expect ~10 Hz
ros2 topic hz /odom     # expect ~20 Hz
ros2 run tf2_ros tf2_echo base_link laser  # should show transform
```

If any of these fail, fix before proceeding.

### 3. Launch SLAM Toolbox

```bash
# Terminal 3
ros2 launch slam_toolbox online_async_launch.py \
    params_file:=$HOME/ros2_ws/src/johnny5/johnny5_bringup/config/slam_params.yaml
```

If no custom params file exists yet, use defaults:

```bash
ros2 launch slam_toolbox online_async_launch.py
```

### 4. Verify map topic

```bash
# Terminal 2
ros2 topic list | grep map     # expect /map
ros2 topic hz /map             # may be slow initially (updates on movement)
```

### 5. Drive the robot to build a map

```bash
# Terminal 4 — keyboard teleop
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
    --ros-args -r /cmd_vel:=/cmd_vel_joy
```

Drive slowly around the room. After covering the area:

```bash
ros2 topic echo /map --no-arr | head -5  # should show map data
```

### 6. Save the map

```bash
mkdir -p ~/maps
ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap \
    "{name: {data: '$HOME/maps/test_map_$(date +%Y%m%d_%H%M%S)'}}"
```

Verify files created:

```bash
ls -la ~/maps/test_map_*.yaml ~/maps/test_map_*.pgm
```

### 7. Record pass evidence

```bash
# Save topic list
ros2 topic list -t > /tmp/demo_c3_topics.txt

# Copy map files
cp ~/maps/test_map_*.yaml /tmp/demo_c3_map.yaml 2>/dev/null || true
cp ~/maps/test_map_*.pgm /tmp/demo_c3_map.pgm 2>/dev/null || true

# TF tree
ros2 run tf2_tools view_frames 2>/dev/null
mv frames.pdf /tmp/demo_c3_frames.pdf 2>/dev/null || true

echo "Demo C3 evidence saved to /tmp/demo_c3_*.{txt,yaml,pgm,pdf}"
```

## Exit Criteria

- [ ] /map topic grows as robot is driven around the room
- [ ] Map saved successfully (.yaml + .pgm files exist and are non-empty)
- [ ] TF chain map → odom → base_link → laser is complete

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| SLAM doesn't start | Missing /scan or /odom | Verify both topics with `ros2 topic hz` |
| Map doesn't update | TF chain broken | `ros2 run tf2_ros tf2_echo map base_link` — fix missing links |
| Map looks wrong | LiDAR tilted | Verify LiDAR is horizontal (≤ 2° tilt) |
| "Extrapolation into the past" | Clock wrong | `sudo date -s "YYYY-MM-DD HH:MM:SS"` |
| Loop closure fails | Room too small | Reduce `loop_match_minimum_chain_size` in slam_params.yaml |
