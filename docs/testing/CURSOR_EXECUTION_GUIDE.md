# Johnny-5 Sensor Tests & Demos — Cursor Execution Guide

> **Purpose:** This document is designed to be fed to an AI coding assistant (Cursor, Claude, etc.)
> working on the Johnny-5 repo. It contains everything needed to programmatically create, place,
> and validate all test artifacts from `SENSOR_TESTS_AND_DEMOS_TASKS.md`.
>
> **How to use in Cursor:** Open this file as context. Ask Cursor to execute sections by name
> (e.g., "Execute TASK-D1" or "Create all files from TASK-D3"). Each task section is self-contained
> with exact file paths, complete file contents (or references to companion files), and verification
> commands.

---

## Repo Layout Context

```
~/ros2_ws/src/johnny5/                     # Main repo root
├── docs/delivery/                         # PBI docs, task files
│   ├── SENSOR_TESTS_AND_DEMOS_TASKS.md    # The source task list
│   ├── JETSON_SETUP_CURRENT_STATE.md      # Current Jetson state
│   ├── demo_runbooks/                     # NEW — demo runbooks (D4)
│   └── sensor_test_checklist.md           # NEW — printable checklist (D5)
├── scripts/                               # NEW — test scripts (D1, D2)
│   ├── sensor_check_hardware.sh           # D1: hardware-level checks
│   └── sensor_check_ros.sh               # D2: ROS topic-level checks
├── johnny5_bringup/
│   └── launch/
│       └── sensor_test.launch.py          # NEW — D3: sensor-only launch
├── jetson/
│   ├── jetson_bridge.py
│   └── ros2/
│       └── balance_bridge/
├── teensy_balance_cascaded/
└── tuning_code/
```

**Convention:** All new files go into the repo tree at paths shown above. Scripts are bash,
launch files are Python (ROS 2 launch), docs are markdown.

---

## TASK-D1: Create Hardware Sensor Check Script

**File:** `scripts/sensor_check_hardware.sh`  
**What:** Pure bash script that checks lsusb, /dev nodes, ALSA devices. No ROS needed.  
**When to run:** First thing — before any ROS tests. Takes < 2 seconds.

### File content

The complete script is provided in the companion file `scripts/sensor_check_hardware.sh`.
Copy it to `~/ros2_ws/src/johnny5/scripts/sensor_check_hardware.sh` and make executable:

```bash
chmod +x ~/ros2_ws/src/johnny5/scripts/sensor_check_hardware.sh
```

### What it checks

| Check ID | Sensor | Method | PASS condition |
|----------|--------|--------|---------------|
| A1.1 | Teensy | `lsusb \| grep 16c0` | VID 16c0 present |
| A1.2 | Teensy | `ls /dev/ttyACM*` | At least one ACM device |
| A2.1 | LiDAR | `lsusb \| grep 10c4` | VID:PID 10c4:ea60 present |
| A2.2 | LiDAR | `ls -la /dev/ldlidar` | Symlink exists |
| A3.1 | OAK-D | `lsusb \| grep 03e7` | VID 03e7 present |
| A3.2 | OAK-D | `python3 -c "import depthai ..."` | ≥1 device found |
| A4.1 | ReSpeaker | `arecord -l` | USB audio device listed |
| A5.1 | ESP32 | `ls /dev/ttyUSB*` | Device present (not LiDAR) |
| — | USB conflict | Count CP210x devices | Warn if >1 |

### Verification

```bash
cd ~/ros2_ws/src/johnny5
bash scripts/sensor_check_hardware.sh
# Exit code 0 = all pass, 1 = failures present
```

---

## TASK-D2: Create ROS Topic Sensor Check Script

**File:** `scripts/sensor_check_ros.sh`  
**What:** Launches ROS drivers in background, checks topic existence and rate, kills drivers.  
**When to run:** After D1 passes. Requires ROS 2 environment.

### File content

The complete script is provided in the companion file `scripts/sensor_check_ros.sh`.
Copy to `~/ros2_ws/src/johnny5/scripts/sensor_check_ros.sh` and make executable.

### Usage

```bash
cd ~/ros2_ws/src/johnny5

# Test individual sensors
bash scripts/sensor_check_ros.sh teensy
bash scripts/sensor_check_ros.sh lidar
bash scripts/sensor_check_ros.sh oak

# Test all individually (sequential)
bash scripts/sensor_check_ros.sh all

# Joint tests
bash scripts/sensor_check_ros.sh combo bridge+lidar
bash scripts/sensor_check_ros.sh combo all_sensors
```

### What it checks per mode

**teensy:**
- Launches `balance_bridge.launch.py` in background
- Checks `/imu/roll` exists and publishes at 10–30 Hz
- Checks `/odom` exists

**lidar:**
- Launches `ld19.launch.py` in background
- Checks `/scan` exists and publishes at 8–12 Hz
- Checks `/scan` frame_id field

**oak:**
- Checks `depthai_ros_driver` package is built
- Launches `driver.launch.py` in background
- Discovers image topics dynamically (namespace may vary)
- Checks image topic rate > 5 Hz

**combo bridge+lidar:**
- Launches both simultaneously
- Checks `/odom` and `/scan` both active
- Checks `base_link → laser` TF exists

**combo all_sensors:**
- Launches bridge + LiDAR + OAK-D (if built)
- Checks all topics
- Saves TF tree to `/tmp/sensor_check_frames.pdf`
- Dumps full `ros2 topic list`

### Logs

All launch output goes to `/tmp/sensor_check_<label>.log`. Check these on failure.

---

## TASK-D3: Create Sensor Test Launch File

**File:** `johnny5_bringup/launch/sensor_test.launch.py`  
**What:** ROS 2 launch file that starts only requested sensors. No SLAM, Nav2, or EKF.  
**When to use:** Integration testing, verifying sensors work together before adding nav stack.

### File content

The complete launch file is provided in the companion file `launch/sensor_test.launch.py`.
Copy to `~/ros2_ws/src/johnny5/johnny5_bringup/launch/sensor_test.launch.py`.

### Rebuild after adding

```bash
cd ~/ros2_ws
colcon build --packages-select johnny5_bringup
source install/setup.bash
```

### Usage

```bash
# Bridge + LiDAR (default)
ros2 launch johnny5_bringup sensor_test.launch.py

# Add OAK-D
ros2 launch johnny5_bringup sensor_test.launch.py enable_oakd:=true

# LiDAR only
ros2 launch johnny5_bringup sensor_test.launch.py enable_bridge:=false

# Custom laser mount offsets
ros2 launch johnny5_bringup sensor_test.launch.py \
    laser_x:=0.1 laser_y:=0.0 laser_z:=0.15
```

### Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `enable_bridge` | `true` | Start balance_bridge_node |
| `enable_lidar` | `true` | Start LDROBOT LiDAR driver |
| `enable_oakd` | `false` | Start OAK-D depthai_ros_driver |
| `enable_static_tf` | `true` | Publish base_link → laser static TF |
| `teensy_device` | `''` | Teensy serial device (empty = auto) |
| `lidar_port` | `/dev/ldlidar` | LiDAR serial port |
| `laser_x/y/z` | `0.0/0.0/0.15` | Laser TF offset from base_link (metres) |

### Key difference from balance_bridge.launch.py

`sensor_test.launch.py` does NOT start `twist_mux`, `esp32_joy_node`, or `teleop_twist_joy`.
It's purely for sensor validation. Use `balance_bridge.launch.py` for driving the robot.

---

## TASK-D4: Create Demo Runbooks

**Directory:** `docs/delivery/demo_runbooks/`  
**What:** One markdown file per demo with prerequisites, commands, exit criteria, troubleshooting.

### Files to create

| File | Demo | Status |
|------|------|--------|
| `demo_01_balance.md` | C1: Balance + serial | **Provided** — see companion file |
| `demo_02_web_teleop.md` | C2: Web teleop + camera | Template below |
| `demo_03_slam.md` | C3: SLAM (LiDAR) | **Provided** — see companion file |
| `demo_04_voice.md` | C4: Voice commands | Template below |
| `demo_05_wake_word.md` | C5: Wake word | Template below |
| `demo_06_nav2.md` | C6: Vision autonomy (Nav2) | Template below |
| `demo_07_follow.md` | C7: Human following | Template below |
| `demo_08_objects.md` | C8: Object recognition | Template below |

### Template for demos that need development

For C2, C4–C8, the demo runbooks should follow this template. Cursor can generate
full runbooks from this once the ROS nodes are developed:

```markdown
# Demo Runbook: C<N> — <Demo Name>

**PBI:** PBI-<N> (Wave <X>)
**Hardware:** <list>
**Prerequisite tests:** <which A/B tests must pass>
**Prerequisite demos:** <which prior C demos must pass>

---

## Prerequisites

- [ ] <hardware connected>
- [ ] <packages built>
- [ ] <prior demos passing>

## Steps

### 1. Launch prerequisites
<exact ros2 launch commands>

### 2. Launch demo-specific nodes
<exact ros2 run/launch commands>

### 3. Execute demo scenario
<what to do physically / what commands to send>

### 4. Verify exit criteria
<what to check>

### 5. Record pass evidence
<commands to save logs/screenshots>

## Exit Criteria
- [ ] <criterion 1>
- [ ] <criterion 2>

## Troubleshooting
| Symptom | Cause | Fix |
|---------|-------|-----|
```

---

## TASK-D5: Create Printable Checklist

**File:** `docs/delivery/sensor_test_checklist.md`  
**What:** Single-page checkbox document covering all A/B/C tests.

The complete checklist is provided in the companion file `docs/sensor_test_checklist.md`.
Copy to `~/ros2_ws/src/johnny5/docs/delivery/sensor_test_checklist.md`.

---

## TASK-INSTALL: File Placement Commands

Run these on the Jetson (or commit to repo and pull) to install all artifacts:

```bash
REPO=~/ros2_ws/src/johnny5

# Create directories
mkdir -p $REPO/scripts
mkdir -p $REPO/docs/delivery/demo_runbooks

# Copy scripts (from wherever you've staged them)
cp sensor_check_hardware.sh  $REPO/scripts/
cp sensor_check_ros.sh       $REPO/scripts/
chmod +x $REPO/scripts/sensor_check_hardware.sh
chmod +x $REPO/scripts/sensor_check_ros.sh

# Copy launch file
cp sensor_test.launch.py $REPO/johnny5_bringup/launch/

# Copy docs
cp sensor_test_checklist.md          $REPO/docs/delivery/
cp demo_runbooks/demo_01_balance.md  $REPO/docs/delivery/demo_runbooks/
cp demo_runbooks/demo_03_slam.md     $REPO/docs/delivery/demo_runbooks/

# Rebuild bringup package (for new launch file)
cd ~/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select johnny5_bringup
source install/setup.bash

# Verify
ros2 launch johnny5_bringup sensor_test.launch.py --show-args
```

---

## EXECUTION SEQUENCE — What to Tell Cursor

Use these as prompts in Cursor, in order. Each assumes the previous completed successfully.

### Phase 1: Create all files

```
Create the following files in the Johnny-5 repo:
1. scripts/sensor_check_hardware.sh — hardware detection script (TASK-D1)
2. scripts/sensor_check_ros.sh — ROS topic validation script (TASK-D2)
3. johnny5_bringup/launch/sensor_test.launch.py — sensor-only launch (TASK-D3)
4. docs/delivery/sensor_test_checklist.md — printable checklist (TASK-D5)
5. docs/delivery/demo_runbooks/demo_01_balance.md — balance demo runbook
6. docs/delivery/demo_runbooks/demo_03_slam.md — SLAM demo runbook
Use the complete file contents from the Cursor Execution Guide.
```

### Phase 2: Validate on Jetson (SSH)

```
SSH to the Jetson and run the hardware check:
  bash ~/ros2_ws/src/johnny5/scripts/sensor_check_hardware.sh
Report which sensors pass and which fail. For failures, run the
diagnostic commands shown in the script output and report what you find.
```

### Phase 3: ROS validation

```
On the Jetson, run the ROS sensor checks:
  bash ~/ros2_ws/src/johnny5/scripts/sensor_check_ros.sh all
Then run joint tests:
  bash ~/ros2_ws/src/johnny5/scripts/sensor_check_ros.sh combo bridge+lidar
  bash ~/ros2_ws/src/johnny5/scripts/sensor_check_ros.sh combo all_sensors
Report results and any /tmp/sensor_check_*.log contents for failures.
```

### Phase 4: First demo

```
Execute the C1 balance demo runbook at
docs/delivery/demo_runbooks/demo_01_balance.md.
Run each step and report results. Save evidence files to /tmp/demo_c1_*.
```

### Phase 5: Create remaining demo runbooks

```
Using the template from TASK-D4 in the Cursor Execution Guide, create
demo runbooks for:
  - C2 (web teleop) — PBI-12, needs rosbridge + HTML joystick page
  - C4 (voice) — PBI-10, needs voice_bridge ROS node
  - C5 (wake word) — PBI-14, needs openWakeWord integration
  - C6 (Nav2) — PBI-8, needs saved map + Nav2 config
  - C7 (human following) — PBI-9, needs person detector on OAK-D
  - C8 (object recognition) — PBI-11, needs YOLOv5/v8 on OAK-D

For each, clearly separate "existing infrastructure" steps from
"development needed" steps. Reference the specific PBI PRD for
technical approach details.
```

### Phase 6: Resolve open questions

```
Run these diagnostic commands on the Jetson and report results:

1. ReSpeaker model:
   lsusb | grep 2886

2. ESP32 vs LiDAR conflict:
   for dev in /dev/ttyUSB*; do
     echo "=== $dev ==="
     udevadm info -a -n $dev | grep -E 'ATTR{serial}|ATTR{product}|ATTR{devpath}'
   done

3. OAK-D topic namespace:
   ros2 launch depthai_ros_driver driver.launch.py &
   sleep 10
   ros2 topic list | grep -iE 'oak|camera|rgb|depth'
   kill %1

4. Clock check (important for TF):
   date
   timedatectl status
```

---

## APPENDIX A: Udev Rule for ESP32 (if needed)

If the ESP32 and LiDAR both show as CP210x (10c4:ea60), create a udev rule
that differentiates by serial number. Run the diagnostic from Phase 6 first.

**File:** `/etc/udev/rules.d/51-esp32.rules`

```bash
# Template — replace <SERIAL> with actual ESP32 serial from udevadm
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{serial}=="<SERIAL>", \
    SYMLINK+="esp32", MODE="0666"
```

Then:

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
ls -la /dev/esp32      # should be symlink to ttyUSBx
ls -la /dev/ldlidar    # should be symlink to a different ttyUSBx
```

Update `esp32_joy_node` device parameter to use `/dev/esp32`.

---

## APPENDIX B: SLAM Params Tuning Notes

When creating `slam_params.yaml` for the balance robot, these parameters
differ from a typical ground robot:

```yaml
slam_toolbox:
  ros__parameters:
    # Balance robot moves slowly and may oscillate
    minimum_travel_distance: 0.3    # metres (default 0.5 — reduce for slow robot)
    minimum_travel_heading: 0.3     # radians (default 0.5 — reduce)

    # Small indoor environments
    loop_match_minimum_chain_size: 5  # default 10 — reduce for small rooms

    # Conservative — balance robot has limited scan range due to height
    max_laser_range: 12.0     # metres (LD19 max is 25, but useful range indoors is less)
    min_laser_range: 0.15     # metres (LD19 min is 0.02, but ignore very close)

    # Map resolution
    resolution: 0.05          # metres/pixel (5 cm — good for indoor nav)

    # Odometry
    use_scan_matching: true
    use_scan_barycenter: true

    # TF frames — must match your TF tree
    odom_frame: odom
    map_frame: map
    base_frame: base_link
    scan_topic: /scan
```

Save to: `~/ros2_ws/src/johnny5/johnny5_bringup/config/slam_params.yaml`

---

## APPENDIX C: Missing Nodes to Develop

These ROS 2 nodes don't exist yet and are needed for demos C2, C4, C5:

### C2: Web Teleop — HTML Joystick Page

**Needed:** Static HTML/JS file served by Jetson that:
- Connects to rosbridge WebSocket at `ws://<JETSON_IP>:9090`
- Publishes `geometry_msgs/Twist` to `/cmd_vel_web`
- Subscribes to compressed camera image topic
- Displays camera feed + virtual joystick

**Libraries:** `roslibjs`, `nipplejs` (virtual joystick), `ros2djs` or raw ImageData

**File location:** `~/ros2_ws/src/johnny5/jetson/web/teleop.html`

### C4: Voice Bridge Node

**Needed:** ROS 2 Python node that:
- Records from ReSpeaker (ALSA device)
- Runs Whisper STT (faster_whisper, small.en, CUDA)
- Parses commands ("go forward", "stop", "turn left", "turn right", "follow me")
- Publishes `geometry_msgs/Twist` to `/cmd_vel_voice`
- Publishes `std_msgs/String` to `/voice/command` (raw text)

**File location:** `~/ros2_ws/src/johnny5/jetson/ros2/balance_bridge/voice_bridge_node.py`

**Add to setup.py entry_points:**
```python
'voice_bridge_node = balance_bridge.voice_bridge_node:main',
```

### C5: Wake Word Integration

**Needed:** Extension of voice_bridge_node that:
- Runs `openWakeWord` continuously on ReSpeaker input
- On "Hey Johnny" detection, opens 5-second Whisper STT window
- Otherwise silent (no STT processing)

**Library:** `openwakeword` (already in bundle)

---

## APPENDIX D: Bundle Updates (if missing packages)

If any packages are missing from the offline bundle, add them on the internet-connected
host and transfer:

```bash
# On host (internet)
cd ~/j5_bundle/pip_wheels
pip download --platform linux_aarch64 --python-version 310 \
    --only-binary=:all: -d . <package_name>

# Transfer to Jetson USB/SD
# On Jetson
pip install --no-index --find-links=/mnt/j5bundle/pip_wheels <package_name> \
    --break-system-packages
```

Known packages that may need adding for demos:
- `nipplejs` (npm, for web joystick — not pip)
- `openwakeword` (should be in bundle)
- `piper-tts` (for TTS in voice demos — optional)
