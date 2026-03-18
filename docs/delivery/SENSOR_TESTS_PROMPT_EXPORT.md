# Sensor Tests & Demos — Prompt Export for Claude / ChatGPT

Copy the sections below into Claude or ChatGPT to get detailed step-by-step instructions, scripts, or runbooks. Use one block per conversation or combine as needed.

---

## Block 1: Context (paste first)

```
I have a robotics project (Johnny-5) on a Jetson AGX Orin running Ubuntu 22.04 and ROS 2 Humble. The stack includes:

- **Teensy 4.1**: Balance controller; talks to Jetson over USB serial. A Python "balance_bridge" node publishes ROS 2 topics: /odom, /robot_state, /imu/roll, /imu/pitch, /imu/yaw and subscribes to /cmd_vel.
- **LDROBOT STL-19P LiDAR**: USB serial (CP210x). ROS 2 driver: ldlidar_stl_ros2, launch ld19.launch.py, default device /dev/ldlidar. Publishes /scan.
- **Luxonis OAK-D Pro**: Depth camera. ROS 2 driver: depthai_ros_driver (depthai-ros). Topics under /oak/ (e.g. rgb, depth).
- **ReSpeaker microphone array**: USB Audio. Used for ASR (e.g. Whisper/faster_whisper) and future wake word. For direction of sound we have respeaker/mic_array (DOA), webrtcvad, numpy, pyaudio.
- **ESP32**: PS3 controller bridge over serial; esp32_joy_node publishes /joy.

Workspace: ~/ros2_ws. Source before any ROS commands: source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash.
Bundle (offline install) at /mnt/j5bundle; verify script: sudo /mnt/j5bundle/scripts/08_verify_install.sh (7 checks: ros2 doctor, johnny packages, balance_bridge package, LiDAR in lsusb, OAK-D in lsusb, Teensy in lsusb, bridge topics visible).
```

---

## Block 2: Individual sensor test instructions

```
Using the context above, give me a full list of step-by-step instructions to test each sensor **individually** on the Jetson:

1. **Teensy / Balance bridge** — How to verify USB, serial device, bridge connection, and that /odom, /imu/roll, /robot_state (or equivalent) are publishing. How to test that /cmd_vel commands reach the robot.
2. **LiDAR (LDROBOT STL-19P)** — How to verify USB (lsusb 10c4), udev symlink /dev/ldlidar, launch the driver, and confirm /scan is publishing at expected rate and has valid data.
3. **OAK-D (depth camera)** — How to verify USB (lsusb 03e7), Python depthai sees the device, ROS 2 depthai_ros_driver is built and launched, and image/depth topics are publishing.
4. **ReSpeaker** — How to verify ALSA device (arecord -l), record a short clip, play it back, and optionally run Whisper transcription. If possible, include how to test direction-of-arrival (DOA) using respeaker/mic_array or similar.
5. **ESP32 / PS3** — How to verify serial device, see JSON from the controller, and confirm /joy is published when running esp32_joy_node.

For each sensor, include: exact commands, expected output, and a one-line "PASS" criterion I can check off.
```

---

## Block 3: Joint sensor tests and integration

```
Using the same Jetson/ROS 2 Johnny-5 context, give me:

1. A **joint test matrix**: Which combinations of sensors (e.g. Bridge + LiDAR, Bridge + OAK-D, LiDAR + OAK-D, all three) to bring up together, in what order, and how to verify each combination (topics, TF tree if relevant).
2. **TF requirements**: What TF frames are required for Nav2/SLAM (e.g. map → odom → base_link → laser). Which node publishes each, and what to do if one is missing.
3. **One combined launch sequence** for "all sensors" (balance bridge + LiDAR + OAK-D): exact ros2 launch commands and order, and how to check that all expected topics are present.
4. How to run the existing verify script (08) and interpret 5 PASS vs 7 PASS; what each check means.
```

---

## Block 4: Demos and runbooks

```
Using the same Johnny-5 Jetson context, write short **demo runbooks** (prerequisites, steps, success criteria) for:

1. **Balance demo**: Robot balances; /imu/roll and /odom live; optional keyboard teleop to move it.
2. **LiDAR SLAM demo**: Start LiDAR + bridge + SLAM Toolbox; drive around; show /map growing; save map.
3. **OAK-D camera demo**: Start OAK-D driver only; show RGB (and depth if available) in RViz or rqt_image_view.
4. **Voice command demo**: ReSpeaker → Whisper STT → command parsing → publish to /cmd_vel (or /cmd_vel_voice); "go forward" and "stop" move the robot.
5. **Full verify demo**: Run script 08 with all hardware connected; achieve 7 PASS and document what each check does.

For each runbook: prerequisites (hardware plugged in, packages built), exact commands in order, expected output or behavior, and how to "record" that the demo passed (e.g. screenshot, log, or checklist).
```

---

## Block 5: Scripts and automation

```
Using the same Johnny-5 Jetson/ROS 2 context, help me create:

1. A **bash script** that runs only the *individual* sensor checks (no launches that stay in foreground): e.g. lsusb for each device, check /dev nodes, and optionally ros2 topic list/topic hz for each sensor's topic *if* the corresponding node is already running. Output should be PASS/FAIL per check with a summary at the end.
2. A **bash script** that, given an argument (teensy | lidar | oak | respeaker | esp32), runs the appropriate launch file in the background, waits a few seconds, runs the relevant topic checks (topic list, topic hz), then kills the launch and prints PASS/FAIL. Use source /opt/ros/humble/setup.bash and source ~/ros2_ws/install/setup.bash inside the script.
3. A **markdown checklist** (single page) that I can print or keep open: one line per test from the individual and joint tests, with boxes to tick and space for date/signer. Group by sensor and then "Joint" and "Demos".
```

---

## Block 6: ReSpeaker direction of sound (DOA)

```
I have a SEEED ReSpeaker microphone array on a Jetson (Ubuntu 22.04, Python 3). I want to determine the **direction of sounds** (Direction of Arrival, DOA).

1. What **libraries and system packages** do I need? (e.g. respeaker/mic_array, webrtcvad, numpy, pyaudio, portaudio — list exact apt and pip installs.)
2. What is the **minimal Python example** to read from the ReSpeaker and print DOA angle (or direction) in real time? Include device selection if multiple mics exist.
3. How do I **expose DOA as a ROS 2 topic** (e.g. a Float32 or custom message with angle in degrees) so other nodes can use it? Prefer Python and rclpy.
4. Any **ReSpeaker model-specific** notes (2-mic vs 4-mic vs 6-mic USB) for DOA accuracy and channel count.
```

---

## Block 7: Offline and bundle alignment

```
My Jetson runs **offline** with an install bundle at /mnt/j5bundle (scripts 01–08). I need sensor tests and demos to work without internet.

1. What **Python packages** for sensors (e.g. depthai, pyaudio, webrtcvad, respeaker-related) must be in the bundle or installed from the bundle's pip_wheels so that LiDAR, OAK-D, and ReSpeaker tests all run?
2. How do I **add a new "sensor test" script** to the bundle (e.g. 09_sensor_tests.sh) that runs the individual sensor checks and writes results to a file, without requiring network?
3. Suggest a **bundle layout** for "demo runbooks" or "test scripts": should they live in /mnt/j5bundle/scripts/, a docs/ folder on the bundle, or in the repo only and copied separately?
```

---

## How to use

- Paste **Block 1** first in a new chat so the model has project context.
- Then paste **Block 2**, **3**, **4**, **5**, **6**, or **7** (or combine 2+3, 4+5) to get instructions.
- If the model asks for paths or package names, refer it to "workspace ~/ros2_ws", "bundle /mnt/j5bundle", "balance_bridge", "ldlidar_stl_ros2", "depthai_ros_driver", "ReSpeaker".
- For a **single consolidated request**, paste Block 1 and then: "Using the context above, produce a single markdown document that contains: (1) full step-by-step instructions for testing each sensor individually, (2) joint test steps for Bridge+LiDAR, Bridge+OAK-D, and all three, (3) short runbooks for balance demo, LiDAR SLAM demo, OAK-D demo, and full 08 verify demo, and (4) a one-page printable checklist of all tests."
```
