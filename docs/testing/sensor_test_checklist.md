# Johnny-5 Sensor Test Checklist

**Date:** _______________  **Tester:** _______________  **Jetson IP:** _______________

---

## Prerequisites

- [ ] SSH to Jetson working
- [ ] `source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash` runs without error
- [ ] tmux session started (`tmux new-session -s sensors`)
- [ ] All USB devices physically connected (Teensy, LiDAR, OAK-D, ReSpeaker, ESP32)

---

## Part A — Individual Sensor Tests

### A1. Teensy / Balance Bridge

| # | Test | Command | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| A1.1 | Teensy in lsusb | `lsusb \| grep 16c0` | Line with PJRC Teensy | ☐ P ☐ F | |
| A1.2 | Serial device | `ls /dev/ttyACM*` | /dev/ttyACM0 exists | ☐ P ☐ F | Device: _______ |
| A1.3 | Bridge connects | `python3 jetson/jetson_bridge.py` → type `s` | `connected: True` | ☐ P ☐ F | Roll: _______ |
| A1.4 | Roll topic | `ros2 topic hz /imu/roll` | 15–25 Hz | ☐ P ☐ F | Rate: _______ Hz |
| A1.5 | Odometry | `ros2 topic echo /odom --no-arr` | Pose updates when moved | ☐ P ☐ F | |
| A1.6 | cmd_vel | `ros2 topic pub --once /cmd_vel_joy ...` | Robot responds physically | ☐ P ☐ F | |
| A1.7 | Bridge topics | `ros2 topic list \| grep -E 'odom\|imu/roll'` | Topics present | ☐ P ☐ F | |

### A2. LiDAR (LDROBOT STL-19P)

| # | Test | Command | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| A2.1 | CP210x in lsusb | `lsusb \| grep 10c4` | 10c4:ea60 present | ☐ P ☐ F | |
| A2.2 | Udev symlink | `ls -la /dev/ldlidar` | Symlink to ttyUSBx | ☐ P ☐ F | Target: _______ |
| A2.3 | Launch driver | `ros2 launch ldlidar_stl_ros2 ld19.launch.py` | No fatal errors | ☐ P ☐ F | |
| A2.4 | Scan rate | `ros2 topic hz /scan` | 8–12 Hz | ☐ P ☐ F | Rate: _______ Hz |
| A2.5 | Scan content | `ros2 topic echo /scan --once` | ranges non-empty | ☐ P ☐ F | frame_id: _______ |
| A2.6 | Via sensors.launch | `ros2 launch johnny5_bringup sensors.launch.py` | Same as A2.3–A2.5 | ☐ P ☐ F | |

### A3. OAK-D Pro

| # | Test | Command | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| A3.1 | Myriad X in lsusb | `lsusb \| grep 03e7` | VID 03e7 present | ☐ P ☐ F | |
| A3.2 | DepthAI Python | `python3 -c "import depthai as dai; ..."` | ≥1 device found | ☐ P ☐ F | Count: _______ |
| A3.3 | ROS driver built | `ros2 pkg list \| grep depthai` | depthai_ros_driver | ☐ P ☐ F | |
| A3.4 | Launch driver | `ros2 launch depthai_ros_driver driver.launch.py` | No crash | ☐ P ☐ F | |
| A3.5 | Image topics | `ros2 topic list \| grep -E 'oak\|rgb\|depth'` | RGB + depth topics | ☐ P ☐ F | Topics: _______ |
| A3.6 | Image rate | `ros2 topic hz <image_topic>` | > 10 Hz | ☐ P ☐ F | Rate: _______ Hz |

### A4. ReSpeaker Microphone

| # | Test | Command | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| A4.1 | ALSA device | `arecord -l` | USB Audio listed | ☐ P ☐ F | Card: _______ |
| A4.2 | Record test | `arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/t.wav` | File > 90 KB | ☐ P ☐ F | |
| A4.3 | Playback | `aplay /tmp/t.wav` | Audio audible | ☐ P ☐ F | |
| A4.4 | Whisper STT | Python faster_whisper transcribe | Recognisable text | ☐ P ☐ F | |
| A4.5 | DOA (optional) | ReSpeaker DOA script | Angle output | ☐ P ☐ F | Model: _______ |

### A5. ESP32 / PS3 Controller

| # | Test | Command | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| A5.1 | USB device | `ls /dev/ttyUSB*` | Device present (not LiDAR) | ☐ P ☐ F | Device: _______ |
| A5.2 | Serial JSON | `minicom -b 115200 -D /dev/ttyUSBx` | JSON when PS3 active | ☐ P ☐ F | |
| A5.3 | Joy topic | `ros2 topic echo /joy` | Joy msgs on stick input | ☐ P ☐ F | |

---

## Part B — Joint / Integration Tests

| # | Test | Sensors | PASS Criterion | Result | Notes |
|---|------|---------|---------------|--------|-------|
| B1 | Bridge + LiDAR | Teensy, LiDAR | /odom + /scan both active; TF base_link→laser | ☐ P ☐ F | |
| B2 | Bridge + OAK-D | Teensy, OAK-D | /odom + OAK-D topics both active | ☐ P ☐ F | |
| B3 | LiDAR + OAK-D | LiDAR, OAK-D | /scan + OAK-D topics; no USB conflicts | ☐ P ☐ F | |
| B4 | Full sensors | All three | /odom + /scan + OAK-D; TF tree consistent | ☐ P ☐ F | |
| B5 | EKF fusion | Teensy (odom+IMU) | /odometry/filtered publishing; no EKF errors | ☐ P ☐ F | |
| B6 | ReSpeaker + bridge | ReSpeaker, Teensy | Audio recorded while bridge running | ☐ P ☐ F | |
| B7 | Verify script 08 | All hardware | 7 PASS in 08_verify_install.sh | ☐ P ☐ F | Score: ___/7 |

---

## Part C — Demos

| # | Demo | PBI | Exit Criteria | Result | Date |
|---|------|-----|--------------|--------|------|
| C1 | Balance + serial | Foundation | /imu/roll live; robot balances on floor | ☐ P ☐ F | |
| C2 | Web teleop + camera | PBI-12 | Browser joystick drives robot; camera visible | ☐ P ☐ F | |
| C3 | SLAM (LiDAR) | PBI-16 | /map grows; map saved | ☐ P ☐ F | |
| C4 | Voice commands | PBI-10 | "go forward"/"stop" work; latency < 2s | ☐ P ☐ F | |
| C5 | Wake word | PBI-14 | >90% detection; <1 false pos/min | ☐ P ☐ F | |
| C6 | Vision autonomy | PBI-8 | 5m×5m loop 3× unassisted | ☐ P ☐ F | |
| C7 | Human following | PBI-9 | Follows person; STOP gesture works | ☐ P ☐ F | |
| C8 | Object recognition | PBI-11 | 5 objects >90% accuracy | ☐ P ☐ F | |

---

## Discovered Values (fill in during testing)

| Item | Value |
|------|-------|
| OAK-D image topic name | |
| OAK-D depth topic name | |
| ReSpeaker ALSA card number | |
| ReSpeaker model (PID) | |
| ESP32 /dev/ttyUSBx device | |
| LiDAR /scan frame_id | |
| Laser TF offsets (x,y,z) | |

---

## Sign-off

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Tester | | | |
| Reviewer | | | |
