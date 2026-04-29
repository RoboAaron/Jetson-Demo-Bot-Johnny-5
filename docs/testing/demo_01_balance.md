# Demo Runbook: C1 — Balance + Serial

**PBI:** Foundation (Wave 0)  
**Hardware:** Teensy 4.1 (USB)  
**Prerequisite tests:** A1.1–A1.7 all PASS

---

## Prerequisites

- [ ] Teensy firmware flashed (`teensy_balance_cascaded.ino`)
- [ ] Teensy connected via USB to Jetson
- [ ] `balance_bridge` package built (`ros2 pkg list | grep balance_bridge`)
- [ ] Robot on floor or balancing stand
- [ ] Safety: someone ready to catch robot

## Steps

### 1. Launch the bridge

```bash
# Terminal 1
source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash
ros2 launch balance_bridge balance_bridge.launch.py
```

Wait for: `balance_bridge_node started` log line.

### 2. Verify IMU topic

```bash
# Terminal 2
ros2 topic echo /imu/roll
```

**Expected:** Float32 values at ~20 Hz.  Values should oscillate around the balance setpoint (approximately -1.6° per TUNING_RECOMMENDATIONS.md).

### 3. Verify odometry

```bash
ros2 topic echo /odom --no-arr | head -20
```

**Expected:** `pose.pose.position.x` values that change when robot moves or wheels spin.

### 4. Verify full topic set

```bash
ros2 topic list | grep -E 'odom|imu|robot_state|balance'
```

**Expected topics:**
- `/odom`
- `/imu/roll`
- `/imu/pitch`
- `/imu/yaw`
- `/balance/vel`
- `/robot_state`

### 5. Test keyboard teleop (optional)

```bash
# Terminal 3
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
    --ros-args -r /cmd_vel:=/cmd_vel_joy
```

Use `i` (forward), `k` (stop), `j` (left), `l` (right).  
**Expected:** Robot responds to commands. Start with small speeds.

### 6. Record pass evidence

```bash
# Save topic list
ros2 topic list -t > /tmp/demo_c1_topics.txt

# Save 10 seconds of roll data
timeout 10 ros2 topic echo /imu/roll > /tmp/demo_c1_roll.txt

# Save topic rates
timeout 5 ros2 topic hz /imu/roll 2>&1 | tail -1 > /tmp/demo_c1_hz.txt
timeout 5 ros2 topic hz /odom 2>&1 | tail -1 >> /tmp/demo_c1_hz.txt

echo "Demo C1 evidence saved to /tmp/demo_c1_*.txt"
```

## Exit Criteria

- [ ] Robot balances on floor (or stand) without falling for 30+ seconds
- [ ] `/imu/roll` streams live data at 15–25 Hz
- [ ] `/odom` updates when robot moves
- [ ] (Optional) Keyboard commands cause observable motion

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `balance_bridge_node` won't connect | Wrong serial device | Check `ls /dev/ttyACM*`, set `device:=/dev/ttyACM1` |
| Roll shows 0.0 constantly | IMU not initialised | Power-cycle Teensy, check BNO085 wiring |
| Robot falls immediately | PID gains wrong | Load EEPROM (`g` command), check TUNING_RECOMMENDATIONS.md |
| No /odom topic | Bridge not connected | Check Teensy USB, look at launch log for errors |
