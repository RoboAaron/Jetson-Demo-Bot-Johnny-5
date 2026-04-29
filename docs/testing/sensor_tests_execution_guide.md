# Johnny-5 Sensor Tests & Demos — Execution Guide

**Author:** Claude (for Aaron)  
**Date:** 2026-03-18  
**Scope:** Step-by-step plan to accomplish every task in `SENSOR_TESTS_AND_DEMOS_TASKS.md`  
**Platform:** Jetson AGX Orin · Ubuntu 22.04 · ROS 2 Humble · Offline bundle at `/mnt/j5bundle`

---

## How to Use This Guide

This guide is organized to match the five parts (A–E) of the task document. For each task, you get: prerequisites, exact commands, expected output, and a PASS/FAIL criterion. Work through Parts A and B first — they validate hardware. Parts C and D build on top. Part E captures decisions you need to make before or during execution.

**Before you start anything**, SSH into the Jetson and run:

```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
```

Use `tmux` for all work so sessions survive SSH drops:

```bash
tmux new-session -s sensors
# Ctrl+B then c = new window
# Ctrl+B then 0-9 = switch windows
```

---

## Part A — Individual Sensor Tests

### A1. Teensy / Balance Bridge

**Prerequisites:** Teensy 4.1 connected via USB, firmware flashed, balance_bridge package built.

#### A1.1 — Teensy visible in lsusb

```bash
lsusb | grep 16c0
```

**Expected:** A line containing `16c0` and `PJRC Teensy`.  
**PASS:** Line present.  
**If FAIL:** Check USB cable (must be data-capable, not charge-only). Try a different USB port on the Jetson. Run `dmesg | tail -20` to see if the kernel detected anything.

#### A1.2 — Serial device present

```bash
ls /dev/ttyACM*
```

**Expected:** `/dev/ttyACM0` (possibly ttyACM1 if other ACM devices are connected).  
**PASS:** At least one `/dev/ttyACM*` device listed.  
**If FAIL:** Teensy firmware may not be flashed, or the device isn't enumerating. Check `dmesg | grep tty`.

#### A1.3 — Bridge connects

```bash
cd ~/ros2_ws/src/johnny5
python3 jetson/jetson_bridge.py
# At the > prompt, type: s
```

**Expected:** Output showing `connected: True` and `roll: <some angle>`.  
**PASS:** `connected: True` appears and roll is a reasonable number (not 0.00 if robot is tilted).  
**If FAIL:** Check that the Teensy serial output format matches what `teensy_comms.py` expects. Try `minicom -b 115200 -D /dev/ttyACM0` to see raw serial output. Type `q` to exit the bridge CLI.

#### A1.4 — Roll topic publishing

In one tmux window, launch the bridge:

```bash
ros2 launch balance_bridge balance_bridge.launch.py
```

In another window:

```bash
ros2 topic echo /imu/roll
```

**Expected:** Float32 values streaming at approximately 20 Hz.  
**Verify rate:**

```bash
ros2 topic hz /imu/roll
```

**PASS:** Rate is 15–25 Hz and values change when the robot is tilted.

#### A1.5 — Odometry updates

```bash
ros2 topic echo /odom --no-arr
```

**Expected:** `pose.pose.position.x` and `pose.pose.position.y` values that change when the robot moves.  
**PASS:** Position values update when the robot is physically moved or wheels spin.

#### A1.6 — cmd_vel reaches robot

```bash
ros2 topic pub --once /cmd_vel_joy geometry_msgs/msg/Twist \
  '{linear: {x: 0.1}, angular: {z: 0.0}}'
```

**Expected:** Robot tilts forward slightly or wheels spin (if balancing). Use `--once` to send a single message, then the watchdog will zero it after 0.5 s.  
**PASS:** Observable physical response from the robot.  
**Safety:** Have someone ready to catch the robot. Start with very small values (0.05–0.1 m/s).

#### A1.7 — Bridge topics in topic list

```bash
ros2 topic list | grep -E 'odom|robot_state|imu/roll'
```

**PASS:** At least `/odom` and `/imu/roll` appear.

---

### A2. LiDAR (LDROBOT STL-19P / LD19)

**Prerequisites:** LiDAR plugged in via USB. Udev rules installed (`99-ldlidar.rules`).

#### A2.1 — USB detected

```bash
lsusb | grep 10c4
```

**Expected:** Line with `10c4:ea60` (CP210x USB-UART bridge).  
**PASS:** Line present.  
**If FAIL:** Try different USB port. Check cable. Run `dmesg | tail -20`.

#### A2.2 — Udev symlink

```bash
ls -la /dev/ldlidar
```

**Expected:** Symlink pointing to `/dev/ttyUSBx`.  
**PASS:** Symlink exists and points to a valid ttyUSB device.  
**If FAIL:** Reload udev rules:

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

If still missing, check `/etc/udev/rules.d/99-ldlidar.rules` exists and has the correct VID/PID.

#### A2.3 — Launch LiDAR driver

```bash
ros2 launch ldlidar_stl_ros2 ld19.launch.py
```

**Expected:** Log output with no serial errors. May say "ldlidar communication is normal" or similar.  
**PASS:** No fatal errors, node stays running.  
**If FAIL:** Try explicit port: `ros2 launch ldlidar_stl_ros2 ld19.launch.py port_name:=/dev/ttyUSB0`. Check baud rate is 230400.

#### A2.4 — Scan topic rate

In a second window:

```bash
ros2 topic hz /scan
```

**Expected:** ~10.0 Hz (the LD19 spins at 10 revolutions/second).  
**PASS:** Rate between 8–12 Hz.

#### A2.5 — Scan content

```bash
ros2 topic echo /scan --once
```

**Expected:** Message with `ranges` array (~502 values), `frame_id` like `base_laser` or `laser`.  
**PASS:** `ranges` array is non-empty, values are between 0.02 and 25.0 (metres).

#### A2.6 — Via bringup launch

```bash
ros2 launch johnny5_bringup sensors.launch.py
```

Then repeat A2.4 and A2.5.  
**PASS:** Same results as launching the driver directly.

---

### A3. OAK-D (Depth Camera)

**Prerequisites:** OAK-D Pro connected via USB-C. `depthai_ros_driver` built (may need to re-run script 04 — see `JETSON_SETUP_CURRENT_STATE.md`).

#### A3.1 — USB detected

```bash
lsusb | grep 03e7
```

**Expected:** Line with Intel Movidius Myriad X.  
**PASS:** Line present.

#### A3.2 — DepthAI Python detection

```bash
python3 -c "import depthai as dai; print(dai.Device.getAllAvailableDevices())"
```

**Expected:** Non-empty list with at least one device.  
**PASS:** List printed with 1+ entries.  
**If FAIL:** Check USB-C cable (must be data-capable). Try different USB port. Verify udev rule `80-movidius.rules` is installed.

#### A3.3 — ROS 2 driver built

```bash
ros2 pkg list | head -100 | grep depthai
```

**Expected:** `depthai_ros_driver` (and possibly `depthai_ros_msgs`, `depthai_bridge`).  
**PASS:** `depthai_ros_driver` appears.  
**If FAIL:** Re-run the build script:

```bash
sudo /mnt/j5bundle/scripts/04_build_ros2_workspace.sh
source ~/ros2_ws/install/setup.bash
```

#### A3.4 — Launch ROS driver

```bash
ros2 launch depthai_ros_driver driver.launch.py
```

**Expected:** Node starts without fatal errors. May take 5–10 seconds to initialize the camera.  
**PASS:** Node running, no crash.

#### A3.5 — Image topics

```bash
ros2 topic list | grep -E 'oak|rgb|depth'
```

**Expected:** Topics like `/oak/rgb/image_raw`, `/oak/stereo/depth`, etc.  
**PASS:** At least one RGB and one depth topic present.

#### A3.6 — Image stream rate

```bash
ros2 topic hz /oak/rgb/image_raw
```

(Adjust topic name based on A3.5 output.)  
**Expected:** ~30 Hz (depends on camera configuration).  
**PASS:** Rate > 10 Hz.

---

### A4. ReSpeaker Microphone

**Prerequisites:** ReSpeaker USB microphone array connected.

#### A4.1 — ALSA device

```bash
arecord -l
```

**Expected:** A card listed as "USB Audio" or "ReSpeaker" or "USB PnP Sound Device".  
**PASS:** At least one USB audio device listed.  
**Note the card number** (e.g., `card 2`) — you'll need it for the next steps.

#### A4.2 — Record test audio

Replace `X` with the card number from A4.1:

```bash
arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/test_audio.wav
```

Speak into the microphone during the 3-second recording.  
**PASS:** File `/tmp/test_audio.wav` exists and is > 90 KB.

#### A4.3 — Playback

```bash
aplay /tmp/test_audio.wav
```

**PASS:** You can hear your recorded voice through connected speakers/headphones.  
**If no speakers:** Check file size (`ls -la /tmp/test_audio.wav`). If it's a reasonable size (90+ KB for 3 seconds at 16 kHz), consider it a pass.

#### A4.4 — Whisper STT

```bash
python3 -c "
import faster_whisper
model = faster_whisper.WhisperModel('small.en', device='cuda', compute_type='float16')
segs, _ = model.transcribe('/tmp/test_audio.wav')
for s in segs:
    print(s.text)
"
```

**Expected:** Text output that roughly matches what you said.  
**PASS:** Recognizable transcription appears.  
**If FAIL:** Try `device='cpu'` if CUDA isn't available. Check that Whisper model files are in `~/.cache/whisper/`.

#### A4.5 — Direction of Arrival (optional)

This depends on the specific ReSpeaker model (2-mic, 4-mic, or 6-mic). The approach varies:

**For 4-mic or 6-mic USB array (has onboard DSP):**

```bash
# Install if not already present
pip install pyusb --break-system-packages

python3 -c "
import usb.core
dev = usb.core.find(idVendor=0x2886, idProduct=0x0018)  # 4-mic: 0x0018, 6-mic: 0x0008
if dev:
    print('ReSpeaker found:', dev)
else:
    print('Not found — check idProduct for your model')
"
```

**PASS:** Device found. Full DOA integration is a PBI-10 task — just confirm the hardware is detectable here.

---

### A5. ESP32 / PS3 Controller

**Prerequisites:** ESP32 connected via USB, flashed with `esp32/ps3_bridge/ps3_bridge.ino`. PS3 controller paired.

#### A5.1 — USB device

```bash
ls /dev/ttyUSB*
```

**Expected:** At least one `/dev/ttyUSBx` device.  
**PASS:** Device present.  
**Potential conflict:** If the LiDAR is also connected, both may use CP210x (same VID:PID). Check which is which:

```bash
udevadm info -a -n /dev/ttyUSB0 | grep -E 'serial|product'
udevadm info -a -n /dev/ttyUSB1 | grep -E 'serial|product'
```

The LiDAR should have a udev symlink at `/dev/ldlidar`. The ESP32 is the other one.

#### A5.2 — Serial JSON output

```bash
# Replace ttyUSBx with the ESP32 device (not the LiDAR)
minicom -b 115200 -D /dev/ttyUSBx
```

**Expected:** JSON packets when the PS3 controller is connected and buttons/sticks are moved. Format: `{"lx":0.0,"ly":0.0,"rx":0.0,"ry":0.0,...,"connected":1}`  
**PASS:** JSON lines appear when controller is active.  
**Exit minicom:** Ctrl+A then X.

#### A5.3 — Joy topic

With the balance_bridge launch running (which includes `esp32_joy_node`):

```bash
ros2 topic echo /joy
```

**Expected:** `sensor_msgs/Joy` messages with axes and buttons arrays.  
**PASS:** Messages appear when controller sticks/buttons are used.

---

## Part B — Joint / Integration Tests

**General approach:** Start sensors in separate tmux windows so you can monitor each independently. Kill cleanly with Ctrl+C.

### B1. Bridge + LiDAR

**Start order:**

1. Window 0: `ros2 launch balance_bridge balance_bridge.launch.py`
2. Window 1: `ros2 launch ldlidar_stl_ros2 ld19.launch.py`

**Verify (window 2):**

```bash
# Both topics active
ros2 topic hz /odom
ros2 topic hz /scan

# TF: base_link → laser exists
ros2 run tf2_ros tf2_echo base_link laser
```

**PASS:** Both topics publishing at expected rates. TF echo shows a transform (even if static 0,0,0 — that's fine, it means the static TF publisher is running). If `base_link → laser` is missing, you need to add a static transform publisher to your launch file:

```bash
ros2 run tf2_ros static_transform_publisher \
  0.1 0 0.1 0 0 0 base_link laser
```

(Adjust x, y, z offsets to match your LiDAR mounting position relative to the robot's center.)

### B2. Bridge + OAK-D

**Start order:**

1. Window 0: `ros2 launch balance_bridge balance_bridge.launch.py`
2. Window 1: `ros2 launch depthai_ros_driver driver.launch.py`

**Verify:**

```bash
ros2 topic hz /odom
ros2 topic list | grep oak
ros2 topic hz /oak/rgb/image_raw  # adjust topic name as needed
```

**PASS:** Both `/odom` and OAK-D image topics publishing simultaneously. No errors in either launch window.

### B3. LiDAR + OAK-D (no bridge)

**Start order:**

1. Window 0: `ros2 launch ldlidar_stl_ros2 ld19.launch.py`
2. Window 1: `ros2 launch depthai_ros_driver driver.launch.py`

**Verify:**

```bash
ros2 topic hz /scan
ros2 topic hz /oak/rgb/image_raw
```

**PASS:** Both publish with no interference, no errors, no USB bandwidth issues.  
**If bandwidth issues:** Try connecting LiDAR and OAK-D to different USB controllers on the Jetson (the AGX Orin has multiple USB root hubs).

### B4. Full sensors (no nav)

**Start order:**

1. `ros2 launch balance_bridge balance_bridge.launch.py`
2. `ros2 launch ldlidar_stl_ros2 ld19.launch.py` (or `ros2 launch johnny5_bringup sensors.launch.py`)
3. `ros2 launch depthai_ros_driver driver.launch.py`

**Verify:**

```bash
ros2 topic list | grep -E 'odom|scan|oak|imu'
ros2 run tf2_tools view_frames
# scp the frames.pdf to your laptop to view the TF tree
```

**PASS:** `/odom`, `/scan`, OAK-D topics all present. TF tree shows `odom → base_link` and `base_link → laser` (and ideally `base_link → oak_*` frames from depthai).

### B5. EKF sensor fusion

**Prerequisites:** `johnny5_sensor_fusion` package built, `ekf.yaml` configured.

**Start order:**

1. `ros2 launch balance_bridge balance_bridge.launch.py`
2. `ros2 launch johnny5_sensor_fusion sensor_fusion.launch.py` (or however your EKF is launched)

**Verify:**

```bash
ros2 topic echo /odometry/filtered --no-arr
ros2 topic hz /odometry/filtered
```

**PASS:** `/odometry/filtered` publishing at a reasonable rate (20–50 Hz), no EKF error messages in the launch output. Pose values should be similar to `/odom` but smoother.

### B6. ReSpeaker + Bridge

**Start order:**

1. `ros2 launch balance_bridge balance_bridge.launch.py`
2. Record audio: `arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 5 /tmp/cmd_test.wav`

Say "go forward" during the recording, then run Whisper on it:

```bash
python3 -c "
import faster_whisper
m = faster_whisper.WhisperModel('small.en', device='cuda', compute_type='float16')
segs, _ = m.transcribe('/tmp/cmd_test.wav')
for s in segs:
    print(s.text)
"
```

**PASS:** Whisper transcribes the audio correctly. The voice → cmd_vel pipeline (PBI-10) isn't built yet, so this test just confirms the audio path works alongside the bridge with no USB conflicts.

### B7. Full verify script

With Teensy + LiDAR + OAK-D all plugged in:

```bash
sudo /mnt/j5bundle/scripts/08_verify_install.sh
```

**Expected:** 7 PASS / 0 FAIL when all hardware is connected and the bridge is running.  
**What each check does:**

| Check | What it verifies |
|-------|-----------------|
| ros2 doctor | ROS 2 installation health |
| Johnny packages | johnny5_bringup, johnny5_description, johnny5_sensor_fusion in `ros2 pkg list` |
| balance_bridge package | balance_bridge in `ros2 pkg list` |
| LiDAR / CP210x in lsusb | `10c4:ea60` present in `lsusb` |
| OAK-D / Myriad X in lsusb | `03e7` present in `lsusb` |
| Teensy in lsusb | `16c0` present in `lsusb` |
| Bridge topics | At least one of `/odom`, `/robot_state`, `/imu/roll` in `ros2 topic list` |

---

## Part C — Demos to Develop

These are higher-level deliverables that build on the sensor tests above. Each maps to a PBI.

### C1. Balance + Serial Demo

**PBI:** Foundation (Wave 0)  
**Hardware:** Teensy only  
**Prerequisites:** A1 tests all pass

**Steps:**

1. Place robot on the floor (or a balancing stand for safety).
2. Launch: `ros2 launch balance_bridge balance_bridge.launch.py`
3. Verify: `ros2 topic echo /imu/roll` — values should oscillate around the setpoint as the robot balances.
4. Optional keyboard teleop:

```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r /cmd_vel:=/cmd_vel_joy
```

Use `i` to move forward, `k` to stop, `j`/`l` to turn.

**Exit criteria:** Robot balances on the floor. `/imu/roll` streams live data. Keyboard commands cause observable motion.

### C2. Web Teleop + Camera Demo

**PBI:** PBI-12 (Wave 1)  
**Hardware:** OAK-D, network connection between Jetson and laptop  
**Prerequisites:** A3 tests pass, `rosbridge_suite` installed

**Steps:**

1. Launch bridge: `ros2 launch balance_bridge balance_bridge.launch.py`
2. Launch OAK-D: `ros2 launch depthai_ros_driver driver.launch.py`
3. Launch rosbridge:

```bash
ros2 launch rosbridge_server rosbridge_websocket_launch.xml
```

4. On your laptop browser, connect to Foxglove Studio (https://studio.foxglove.dev) or build a simple HTML joystick page.
5. Connect via WebSocket: `ws://<JETSON_IP>:9090`
6. Verify: Camera feed visible, joystick commands move the robot.

**Exit criteria:** Browser joystick drives robot. Camera stream visible in browser. Latency < 200 ms.

**Development needed:** HTML/JS joystick page that publishes Twist to `/cmd_vel_web` and subscribes to compressed image topic. This is the main PBI-12 development work.

### C3. SLAM (LiDAR) Demo

**PBI:** PBI-16 (Wave 2)  
**Hardware:** LiDAR  
**Prerequisites:** A2 tests pass, B1 passes, SLAM Toolbox installed

**Steps:**

1. Launch bridge + LiDAR + SLAM:

```bash
# Terminal 1
ros2 launch balance_bridge balance_bridge.launch.py

# Terminal 2
ros2 launch ldlidar_stl_ros2 ld19.launch.py

# Terminal 3 — ensure static TF for laser is published
ros2 run tf2_ros static_transform_publisher \
  0.1 0 0.15 0 0 0 base_link laser

# Terminal 4
ros2 launch slam_toolbox online_async_launch.py \
  params_file:=~/ros2_ws/src/johnny5/johnny5_bringup/config/slam_params.yaml
```

2. Drive the robot around (keyboard teleop or PS3 controller).
3. Verify map building:

```bash
ros2 topic echo /map --no-arr | head -5
```

4. Save the map:

```bash
ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap \
  "{name: {data: '/home/robot/maps/first_map'}}"
```

**Exit criteria:** `/map` topic grows as robot drives around. Map saved successfully as `.yaml` + `.pgm` files.

### C4. Voice Commands Demo

**PBI:** PBI-10 (Wave 2B)  
**Hardware:** ReSpeaker, Jetson GPU  
**Prerequisites:** A4 tests pass

**Steps:**

1. Launch bridge: `ros2 launch balance_bridge balance_bridge.launch.py`
2. Run the voice bridge node (to be developed as part of PBI-10):

```bash
ros2 run balance_bridge voice_bridge_node  # future node
```

3. Say "go forward" — robot should move.
4. Say "stop" — robot should halt.

**Exit criteria:** "go forward" moves the robot. "stop" halts. Response latency < 2 seconds.

**Development needed:** `voice_bridge` ROS 2 node (ReSpeaker → Whisper STT → command parser → `/cmd_vel_voice`). This is the core PBI-10 work.

### C5. Wake Word Demo

**PBI:** PBI-14 (Wave 3)  
**Hardware:** ReSpeaker  
**Prerequisites:** C4 (voice commands) working

**Development needed:** Integrate `openWakeWord` with the voice pipeline. Keyword: "Hey Johnny". On detection, activate a 5-second Whisper STT window.

**Exit criteria:** >90% wake word detection rate from 3 metres. < 1 false positive per minute.

### C6. Vision Autonomy (Nav2) Demo

**PBI:** PBI-8 (Wave 3)  
**Hardware:** LiDAR, OAK-D  
**Prerequisites:** C3 (SLAM map saved), PBI-13 (sensor fusion), Nav2 installed

**Steps:**

1. Load saved map and launch Nav2:

```bash
ros2 launch johnny5_bringup robot.launch.py \
  enable_slam:=false \
  enable_nav2:=true \
  map:=/home/robot/maps/first_map.yaml \
  use_rviz:=false
```

2. Send navigation goals via `ros2 action` or from a laptop running RViz.
3. Robot autonomously navigates a 5 m × 5 m loop.

**Exit criteria:** Three laps without manual intervention.

### C7. Human Following Demo

**PBI:** PBI-9 (Wave 3)  
**Hardware:** OAK-D  
**Prerequisites:** A3 tests pass, balance reliable

**Development needed:** MobileNet-SSD person detector on OAK-D MyriadX. Follow controller maintaining 1.0–1.5 m distance. Hand pose classifier for gestures (stop, follow me, spin).

**Exit criteria:** Robot follows a person across a room. Responds to STOP gesture.

### C8. Object Recognition Demo

**PBI:** PBI-11 (Wave 4)  
**Hardware:** OAK-D  
**Prerequisites:** OAK-D integrated

**Development needed:** YOLOv5/v8-nano on OAK-D MyriadX. "Approach and announce" behavior.

**Exit criteria:** Identifies 5 common objects (bottle, chair, person, box, phone) at >90% accuracy.

---

## Part D — Test Artifacts to Create

### D1. Individual Sensor Test Script

Create a script that runs hardware-level checks (lsusb, /dev nodes) without launching ROS nodes in the foreground. This is a quick "are my devices plugged in?" check.

**Location:** `~/ros2_ws/src/johnny5/scripts/sensor_check_hardware.sh`

**What it should do:**

1. Check `lsusb` for Teensy (16c0), LiDAR (10c4), OAK-D (03e7).
2. Check `/dev/ttyACM*` for Teensy.
3. Check `/dev/ldlidar` symlink.
4. Check `arecord -l` for ReSpeaker.
5. Check `/dev/ttyUSB*` for ESP32.
6. Print PASS/FAIL per check and a summary.

**Implementation notes:** Pure bash, no ROS dependencies. Run as `bash sensor_check_hardware.sh`. Should complete in < 2 seconds.

### D2. Joint Sensor Test Script

Launches each sensor's ROS driver in the background, waits for topics, then kills the driver. More involved than D1.

**Location:** `~/ros2_ws/src/johnny5/scripts/sensor_check_ros.sh`

**What it should do:**

1. Accept arguments: `all | teensy | lidar | oak`
2. Source ROS 2 and workspace.
3. Launch the relevant driver in background (`&`).
4. Wait 8–10 seconds for startup.
5. Check `ros2 topic list` and `ros2 topic hz` for expected topics.
6. Kill the background process.
7. Print PASS/FAIL.

**Implementation notes:** Use `timeout` command for `ros2 topic hz` (e.g., `timeout 5 ros2 topic hz /scan`). Capture the output and check if the rate is within range.

### D3. Sensor Test Mode Launch File

A ROS 2 launch file that starts only the requested sensors — no SLAM, no Nav2, no EKF.

**Location:** `~/ros2_ws/src/johnny5/johnny5_bringup/launch/sensor_test.launch.py`

**Arguments:**

- `enable_bridge:=true` — starts balance_bridge_node (no twist_mux, no esp32_joy)
- `enable_lidar:=true` — starts ldlidar driver
- `enable_oakd:=false` — starts depthai_ros_driver
- `enable_static_tf:=true` — publishes base_link → laser static transform

This gives you a minimal "all sensors running" configuration for integration testing.

### D4. Demo Runbooks

Short markdown files, one per demo (C1–C8), each containing:

1. **Prerequisites** — hardware connected, packages built, prior demos passing
2. **Commands** — exact `ros2 launch` / `ros2 run` commands in order
3. **Expected output** — what topics to check, what behavior to observe
4. **Recording a pass** — how to document success (screenshot of `ros2 topic hz`, log file, video)

**Location:** `~/ros2_ws/src/johnny5/docs/delivery/demo_runbooks/`

Files: `demo_01_balance.md`, `demo_02_web_teleop.md`, `demo_03_slam.md`, etc.

### D5. Printable Checklist

A single markdown file with checkbox tables covering A1–A5, B1–B7, and demo status.

**Location:** `~/ros2_ws/src/johnny5/docs/delivery/sensor_test_checklist.md`

**Format:**

```
## Individual Sensors
- [ ] A1.1 Teensy in lsusb          Date: ___  Tester: ___
- [ ] A1.2 Serial device present     Date: ___  Tester: ___
...

## Joint Tests
- [ ] B1 Bridge + LiDAR              Date: ___  Tester: ___
...

## Demos
- [ ] C1 Balance + serial            Date: ___  Tester: ___
...
```

---

## Part E — Open Questions & Decisions

These need answers before or during execution. Some you can answer now, others will become clear during testing.

### E1. ReSpeaker Model

**Question:** Which exact model (2-mic, 4-mic, 6-mic USB)?  
**Impact:** Affects DOA capability, channel count, and which Python libraries to use.  
**How to determine:** When plugged in, run `lsusb` and check the product ID:

| Product ID | Model |
|-----------|-------|
| `2886:0018` | ReSpeaker 4-Mic Array |
| `2886:0008` | ReSpeaker 6-Mic Circular |
| `2886:0010` | ReSpeaker 2-Mic Pi HAT (unlikely via USB) |

**Recommendation:** The 4-mic array is most common for USB. If you have it, DOA accuracy is ±15° which is adequate for voice-activated control.

### E2. ESP32 vs LiDAR USB Conflict

**Question:** Both can use CP210x (same VID:PID 10c4:ea60). How to differentiate?  
**Solution:** Use udev rules with the USB serial number or port path:

```bash
# Find serial numbers
udevadm info -a -n /dev/ttyUSB0 | grep ATTR{serial}
udevadm info -a -n /dev/ttyUSB1 | grep ATTR{serial}
```

Then add a udev rule for the ESP32 based on its serial number:

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{serial}=="<ESP32_SERIAL>", SYMLINK+="esp32", MODE="0666"
```

If serial numbers are identical (some cheap CP210x chips don't have unique serials), use `ATTRS{devpath}` (USB port position) instead:

```bash
udevadm info -a -n /dev/ttyUSB0 | grep devpath
```

### E3. OAK-D Topic Namespace

**Question:** Are topics under `/oak/...` or `/camera/...`?  
**How to determine:** After launching the driver, run `ros2 topic list`. The default namespace for depthai-ros is `/oak/` but can be configured. Document the actual names in your checklist.

### E4. Test Environment

**Question:** Headless only, or with display for RViz/visual demos?  
**Recommendation:** Run headless for all Part A and B tests (SSH only). For demos C2+ that benefit from visualization, use one of:

- **SSH X-forwarding:** `ssh -X jetson` then `rviz2` (slow but works)
- **Foxglove Studio:** Browser-based, connect via rosbridge WebSocket (recommended for regular use)
- **VNC:** If you want a full desktop session

### E5. Offline Requirement

**Question:** Must all tests pass with bundle only (no internet), or allow network for demos?  
**Recommendation:** All Part A and B tests must work fully offline. Part C demos that need additional packages (like the voice pipeline or web interface) should document any extra dependencies needed in the bundle. The existing bundle at `/mnt/j5bundle` should already have everything for A/B tests.

---

## Execution Plan — Recommended Order

Here's the suggested sequence for working through everything:

### Session 1: Hardware Validation (1–2 hours)

1. Connect all hardware (Teensy + LiDAR + OAK-D + ReSpeaker).
2. Run through A1.1–A1.3 (Teensy hardware checks).
3. Run through A2.1–A2.2 (LiDAR hardware checks).
4. Run through A3.1–A3.2 (OAK-D hardware checks).
5. Run through A4.1–A4.3 (ReSpeaker hardware checks).
6. Run `08_verify_install.sh` — target 5+ PASS.
7. **Decision point:** If any hardware isn't detected, troubleshoot before proceeding.

### Session 2: ROS Topic Validation (1–2 hours)

1. Launch balance_bridge → run A1.4–A1.7.
2. Launch LiDAR driver → run A2.3–A2.6.
3. Launch OAK-D driver → run A3.3–A3.6.
4. Run A4.4 (Whisper STT test).
5. If ESP32 is ready: run A5.1–A5.3.
6. **Outcome:** All individual sensor ROS topics validated.

### Session 3: Integration Tests (1–2 hours)

1. B1: Bridge + LiDAR together.
2. B2: Bridge + OAK-D together.
3. B3: LiDAR + OAK-D together (check for USB bandwidth issues).
4. B4: All three together.
5. B5: EKF fusion (if sensor_fusion package is ready).
6. B7: Full verify script with everything plugged in → target 7 PASS.

### Session 4: Create Test Artifacts (2–3 hours, can be done on laptop)

1. Write `sensor_check_hardware.sh` (D1).
2. Write `sensor_check_ros.sh` (D2).
3. Create `sensor_test.launch.py` (D3).
4. Write demo runbook for C1 (D4 — balance demo).
5. Create the printable checklist (D5).

### Session 5: First Demo (C1 — Balance)

1. Run through the C1 runbook.
2. Record success (log output, maybe a short video).
3. This validates Wave 0 exit criteria.

### Future Sessions: Build Toward Wave 1+

- C2 (web teleop) requires building the HTML joystick page → PBI-12 development.
- C3 (SLAM) requires SLAM Toolbox configuration → PBI-16 development.
- C4 (voice) requires the voice_bridge node → PBI-10 development.

---

## Appendix: Quick Command Reference

| What | Command |
|------|---------|
| Source ROS | `source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash` |
| All topics | `ros2 topic list -t` |
| Topic rate | `ros2 topic hz /topic_name` |
| Topic content | `ros2 topic echo /topic_name --once` |
| TF tree | `ros2 run tf2_tools view_frames` |
| Specific TF | `ros2 run tf2_ros tf2_echo frame1 frame2` |
| Node list | `ros2 node list` |
| Package list | `ros2 pkg list \| grep keyword` |
| System health | `ros2 doctor` |
| GPU stats | `tegrastats` |
| USB devices | `lsusb` |
| Kernel log | `dmesg \| tail -30` |
| Kill all ROS | `pkill -f ros2` (nuclear option) |
