# Project Tasks
_Last updated: 2026-02-11_

## PRD Summary

**Project**: Jetson Self-Balancing Robot (Johnny-5)
**Goal**: Carry-on sized, two-wheeled self-balancing robot (~8 mph) powered by NVIDIA Jetson AGX Orin, demonstrating 8 core AI/robotics capabilities.

The PRD is distributed across:
- `robotics_project_overview.md` — goals, hardware, software stack
- `robotics_design_methodology.md` — design principles, iteration phases
- `README.md` — full installation guide and architecture
- `software_extensions.md` — complete software dependency reference

---

## Development Phases

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Mechanical mock-up: plates, mounts, wiring dry-run | In progress (hardware) |
| Phase 2 | Balance testing: Teensy + FSESC only | Not started |
| Phase 3 | Jetson integration: vision, SLAM, navigation | Not started |
| Phase 4 | Autonomy demos: human-follow, voice, teleoperation | Not started |
| Phase 5 | Safety validation: e-stop, recovery routines | Not started |

---

## Task Backlog

### Phase 2 — Embedded / Balance Control (Teensy)

- [ ] **[FIRMWARE]** Write Teensy 4.1 firmware skeleton (PlatformIO project setup)
- [ ] **[FIRMWARE]** Implement BNO085 IMU data acquisition (I2C, BNO085 library)
- [ ] **[FIRMWARE]** Implement complementary/Kalman filter for pitch/roll estimation
- [ ] **[FIRMWARE]** Implement PID balance controller (upright balance loop)
- [ ] **[FIRMWARE]** Implement FSESC CAN bus interface (send motor torque commands)
- [ ] **[FIRMWARE]** Add watchdog timer and e-stop handler on Teensy
- [ ] **[FIRMWARE]** Add serial debug output for balance state (gains, pitch, output)
- [ ] **[TEST]** Manual bench test: verify IMU reads correct orientation
- [ ] **[TEST]** Motor spin test: verify CAN commands drive both motors correctly

### Phase 3 — Jetson / ROS 2 Core

- [ ] **[ROS2]** Create `ros2_ws/` workspace structure in repo
- [ ] **[ROS2]** Create `balance_controller` ROS 2 package (subscribes to IMU, publishes cmd_vel)
- [ ] **[ROS2]** Create `sensor_fusion` ROS 2 package (IMU + odometry → EKF via robot_localization)
- [ ] **[ROS2]** Create `jetson_bringup` launch package with top-level `robot.launch.py`
- [ ] **[ROS2]** Create URDF/xacro robot description (base plate, mast, wheel geometry)
- [ ] **[ROS2]** Integrate OAK-D Pro via `depthai-ros` package (depth + RGB topics)
- [ ] **[ROS2]** Configure SLAM Toolbox for online mapping (2D lidar via ldrobot_lidar_ros2)
- [ ] **[ROS2]** Configure Nav2 with robot-specific footprint and costmap params
- [ ] **[ROS2]** Write ROS 2 → Teensy bridge node (translates `/cmd_vel` to CAN commands)
- [ ] **[SCRIPT]** Create `install_robot_software.sh` (automated full-stack installer)
- [ ] **[SCRIPT]** Create `setup_python_env.sh` (Python virtualenv for non-ROS tools)

### Phase 4 — Autonomy Demos

- [ ] **[DEMO]** Human following: person detection (OAK-D + MobileNet/YOLOv8), proportional follower node
- [ ] **[DEMO]** Gesture control: hand landmark detection, gesture-to-command mapping
- [ ] **[DEMO]** Conversational companion: Whisper ASR pipeline + local LLM (llama.cpp) + TTS
- [ ] **[DEMO]** Wake word detection: Porcupine integration with ReSpeaker array
- [ ] **[DEMO]** Object recognition: YOLOv8 inference on Jetson (TensorRT-optimized)
- [ ] **[DEMO]** Remote teleoperation: Flask + WebSocket web UI with gamepad support
- [ ] **[DEMO]** VR teleoperation: OpenVR / WebXR interface (stretch goal)
- [ ] **[DEMO]** Autonomous waypoint navigation: Nav2 goal sending from web UI

### Phase 5 — Safety & Reliability

- [ ] **[SAFETY]** Fall detection: IMU tilt threshold → motor stop + alarm
- [ ] **[SAFETY]** Auto-balance recovery: controlled re-stand-up sequence
- [ ] **[SAFETY]** Battery voltage monitoring: low-voltage cutoff warning via ROS topic
- [ ] **[SAFETY]** Software e-stop: `/emergency_stop` ROS service disables all actuators
- [ ] **[SAFETY]** Watchdog heartbeat: Jetson → Teensy keepalive; motors stop if lost
- [ ] **[SAFETY]** systemd service for robot auto-start on Jetson boot

### Infrastructure / Repo Health

- [ ] **[INFRA]** Add GitHub Issues templates (bug, feature, hardware-issue)
- [ ] **[INFRA]** Add CI workflow (lint Python, build ROS 2 packages in Docker)
- [ ] **[INFRA]** Add pre-commit hooks (flake8, black, clang-format for C++)
- [ ] **[DOCS]** Document ROS 2 topic/service interface contracts
- [ ] **[DOCS]** Add wiring diagram as editable source (KiCad or draw.io)
- [ ] **[DOCS]** Add CAD files or links (Onshape) for chassis plates

---

## Immediately Workable Tasks (no hardware required)

The following tasks can be done in this repo right now, without physical hardware:

1. **Teensy firmware skeleton** — PlatformIO project with BNO085 stub and CAN stub
2. **ROS 2 workspace + balance_controller package** — Node scaffold, CMakeLists, package.xml
3. **URDF robot description** — Geometry model for visualization in RViz
4. **`install_robot_software.sh`** — Automated installer (from software_extensions.md)
5. **`setup_python_env.sh`** — Python virtualenv setup script
6. **`jetson_bringup` launch package** — Top-level launch file wiring all nodes together
7. **CI workflow** — GitHub Actions to build ROS 2 packages and lint code
8. **GitHub Issues templates** — Standard templates for tracking hardware and software issues

---

## Done

- [x] Hardware BOM and cost tracking (`robotics_inventory_costs.md`, `.csv`)
- [x] Project overview and design methodology documentation
- [x] Software stack reference (`software_extensions.md`)
- [x] Setup guides: Teensy, FSESC, Ubuntu, Windows
- [x] LiDAR ROS 2 package (`ldrobot_lidar_ros2/`) with launch files and SLAM config
- [x] Quick reference command guide
- [x] Power wiring diagram (PDF)
- [x] Installation verification script (`test_installations.py`)
