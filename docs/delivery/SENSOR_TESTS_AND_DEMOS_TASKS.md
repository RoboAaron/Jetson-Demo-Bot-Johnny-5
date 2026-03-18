# Sensor Tests and Demos — Full Task List

**Purpose:** Test each sensor individually, then in combination; define tests and demos that prove functionality.  
**Platform:** Jetson AGX Orin, Ubuntu 22.04, ROS 2 Humble. Bundle at `/mnt/j5bundle`, workspace `~/ros2_ws`.  
**Reference:** `docs/delivery/jetson_headless_guide.md` §7 (Sensor Bring-Up), §9 (Demo Waves); `docs/delivery/JETSON_SETUP_CURRENT_STATE.md`.

---

## Part A — Individual Sensor Tests

### A1. Teensy / Balance Bridge

| # | Task | Command / Action | Success Criterion | Reference |
|---|------|------------------|-------------------|-----------|
| A1.1 | Teensy visible | `lsusb \| grep 16c0` | Line with PJRC Teensy | Headless §7.1 |
| A1.2 | Serial device | `ls /dev/ttyACM*` | e.g. `/dev/ttyACM0` | Headless §7.1 |
| A1.3 | Bridge connects | Run `jetson_bridge.py`, send `s` | `connected: True` | Headless §7.1 |
| A1.4 | Roll topic | `ros2 topic echo /imu/roll` | Float32 at ~20 Hz | Headless §7.1 |
| A1.5 | Odometry | `ros2 topic echo /odom --no-arr` | pose updates when robot moved | Headless §7.1 |
| A1.6 | cmd_vel to robot | `ros2 topic pub /cmd_vel_joy geometry_msgs/Twist '{linear: {x: 0.1}}'` | Robot responds (tilts/moves) | Headless §7.1 |
| A1.7 | Bridge topics | `ros2 topic list \| grep -E 'odom|robot_state|imu/roll'` | At least one present when bridge running | Verify 08 |

**Precondition:** Balance bridge launch: `ros2 launch balance_bridge balance_bridge.launch.py`

---

### A2. LiDAR (LDROBOT STL-19P / LD19)

| # | Task | Command / Action | Success Criterion | Reference |
|---|------|------------------|-------------------|-----------|
| A2.1 | USB detected | `lsusb \| grep 10c4` | CP210x (10c4:ea60) | Headless §7.2 |
| A2.2 | Udev symlink | `ls -la /dev/ldlidar` | Symlink to ttyUSB* | Headless §7.2 |
| A2.3 | Launch driver | `ros2 launch ldlidar_stl_ros2 ld19.launch.py` | Log: "ldlidar communication is normal" (or no serial error) | Headless §7.2 |
| A2.4 | Scan topic | `ros2 topic hz /scan` | ~10 Hz | Headless §7.2 |
| A2.5 | Scan content | `ros2 topic echo /scan` (brief) | `ranges` array, frame_id e.g. base_laser | Headless §7.2 |
| A2.6 | Via sensors launch | `ros2 launch johnny5_bringup sensors.launch.py` | Same as A2.3–A2.5 | sensors.launch.py |

**Note:** Default port is `/dev/ldlidar`; override with `port_name:=/dev/ttyUSB1` if needed.

---

### A3. OAK-D (Depth Camera)

| # | Task | Command / Action | Success Criterion | Reference |
|---|------|------------------|-------------------|-----------|
| A3.1 | USB detected | `lsusb \| grep 03e7` | Intel Movidius Myriad X | Headless §7.3 |
| A3.2 | depthai Python | `python3 -c "import depthai as dai; print(dai.Device.getAllAvailableDevices())"` | Non-empty list | Headless §4.8, §7.3 |
| A3.3 | ROS driver built | `ros2 pkg list \| grep depthai_ros_driver` | Package listed (after 04 re-run if needed) | JETSON_SETUP_CURRENT_STATE |
| A3.4 | Launch driver | `ros2 launch depthai_ros_driver driver.launch.py` | No fatal errors | Headless §7.3 |
| A3.5 | Image topic | `ros2 topic list \| grep -E 'oak|rgb|depth'` | e.g. `/oak/rgb/image_raw`, depth | depthai-ros |
| A3.6 | Image stream | `ros2 topic hz /oak/rgb/image_raw` (or actual topic name) | ~30 Hz or similar | — |

**Precondition:** If `depthai_ros_driver` not built: `sudo /mnt/j5bundle/scripts/04_build_ros2_workspace.sh`

---

### A4. ReSpeaker Microphone

| # | Task | Command / Action | Success Criterion | Reference |
|---|------|------------------|-------------------|-----------|
| A4.1 | ALSA device | `arecord -l` | USB Audio / ReSpeaker listed | Headless §7.4 |
| A4.2 | Record test | `arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/t.wav` | File created (replace X with card) | Headless §7.4 |
| A4.3 | Playback | `aplay /tmp/t.wav` | Audio plays | Headless §7.4 |
| A4.4 | Whisper STT | Use faster_whisper (or whisper) to transcribe `/tmp/t.wav` | Text output | Headless §7.4 |
| A4.5 | DOA (optional) | Use respeaker/mic_array DOA scripts | Direction angle output | ReSpeaker mic_array repo |

**Libraries for DOA:** `respeaker/mic_array`, `webrtcvad`, `numpy`, `pyaudio` (see project notes on ReSpeaker DOA).

---

### A5. ESP32 / PS3 Controller

| # | Task | Command / Action | Success Criterion | Reference |
|---|------|------------------|-------------------|-----------|
| A5.1 | USB device | `ls /dev/ttyUSB*` | Device present (may conflict with LiDAR if same VID:PID) | Headless §7.5 |
| A5.2 | Serial output | `minicom -b 115200 -D /dev/ttyUSBx` with PS3 paired | JSON packets | Headless §7.5 |
| A5.3 | Joy topic | Run `esp32_joy_node`, then `ros2 topic echo /joy` | Joy messages when buttons/sticks used | Headless §7.5 |

---

## Part B — Joint / Integration Tests

| # | Task | Sensors Involved | What to Do | Success Criterion |
|---|------|-------------------|------------|-------------------|
| B1 | Bridge + LiDAR | Teensy, LiDAR | Launch balance_bridge + ld19 (or sensors.launch). Check `/odom`, `/scan` both publishing. | Both topics active; TF `base_link` → `laser` (or base_laser) present |
| B2 | Bridge + OAK-D | Teensy, OAK-D | Launch balance_bridge + depthai_ros_driver. Check `/odom`, `/oak/...` topics. | Odometry and camera streams live |
| B3 | LiDAR + OAK-D | LiDAR, OAK-D | Launch LiDAR + OAK-D drivers only. Check `/scan`, `/oak/...`. | No conflicts; both publish |
| B4 | Full sensors (no nav) | Teensy, LiDAR, OAK-D | balance_bridge + sensors.launch + depthai_ros_driver. | `/odom`, `/scan`, OAK-D topics; TF tree consistent |
| B5 | EKF fusion | Teensy (odom + IMU) | Launch balance_bridge + sensor_fusion (EKF). | `/odometry/filtered` publishing; no EKF errors |
| B6 | ReSpeaker + bridge | ReSpeaker, Teensy | Record from ReSpeaker, run bridge; optional: voice cmd → cmd_vel | Audio recorded; (optional) command moves robot |
| B7 | Verify script 08 | All hardware | Run `sudo /mnt/j5bundle/scripts/08_verify_install.sh` | 7 PASS when Teensy + LiDAR + OAK-D connected |

---

## Part C — Demos to Develop / Document

| # | Demo | Hardware | Goal | Exit Criteria (from Headless §9) |
|---|------|----------|------|-----------------------------------|
| C1 | Balance + serial | Teensy | Robot balances; /imu/roll and /odom live | `ros2 topic echo /imu/roll` live; robot balances on floor |
| C2 | Web teleop + camera | OAK-D, network | Browser joystick drives robot; camera stream in browser | PBI-12 |
| C3 | SLAM (LiDAR) | LiDAR | Map built while driving | `/map` grows; map saved successfully (PBI-16) |
| C4 | Voice commands | ReSpeaker, GPU | “go forward” / “stop” control robot | Response latency < 2 s (PBI-10) |
| C5 | Wake word | ReSpeaker | “Hey Johnny” starts listen window | >90% detection; <1 false positive/min (PBI-14) |
| C6 | Vision autonomy (Nav2) | LiDAR, OAK-D | Autonomous nav 5 m × 5 m loop 3× | No intervention (PBI-8) |
| C7 | Human following | OAK-D | Follow person 1–1.5 m; STOP gesture | PBI-9 |
| C8 | Object recognition | OAK-D | Identify 5 object classes | >90% accuracy (PBI-11) |

---

## Part D — Test Artifacts to Create

| # | Artifact | Description |
|---|----------|-------------|
| D1 | **Shell script: individual sensor tests** | One script (or one per sensor) that runs A1–A5 checks and prints PASS/FAIL. |
| D2 | **Shell script: joint sensor test** | Runs B1–B4 (and optionally B5–B7); reports which combinations work. |
| D3 | **ROS 2 launch: sensor test mode** | Launch file that starts only requested sensors (LiDAR, OAK-D, bridge) for testing. |
| D4 | **Demo runbooks** | Short step-by-step for each of C1–C8: prerequisites, commands, expected output, how to record a “demo pass”. |
| D5 | **Checklist (markdown or printable)** | Single-page checklist of A1–A5 and B1–B7 for hand-off or regression. |

---

## Part E — Open Questions / Decisions

- [ ] ReSpeaker: Which exact model (2-mic, 4-mic, 6-mic USB)? Affects DOA and pipeline.
- [ ] ESP32 vs LiDAR: Both can use CP210x; if on same host, udev rules or port selection to avoid conflict.
- [ ] OAK-D topic namespace: Confirm final topic names after depthai_ros_driver launch (`/oak/...` vs `/camera/...`).
- [ ] Test environment: Headless only, or with display for RViz/visual demos?
- [ ] Offline: All tests must pass with bundle at `/mnt/j5bundle` (no internet) or allow optional network for demos?

---

## Quick Reference — Launch Commands

```bash
# Source (required)
source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash

# Balance bridge only
ros2 launch balance_bridge balance_bridge.launch.py

# LiDAR only (default /dev/ldlidar)
ros2 launch ldlidar_stl_ros2 ld19.launch.py
# Or via bringup
ros2 launch johnny5_bringup sensors.launch.py

# OAK-D only (after depthai_ros_driver built)
ros2 launch depthai_ros_driver driver.launch.py

# Full verify
sudo /mnt/j5bundle/scripts/08_verify_install.sh
```
