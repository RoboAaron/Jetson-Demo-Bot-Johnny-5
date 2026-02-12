# PBI-5 Tasks: Demo Use Case Prioritization
_Status: Done | Completed: 2026-02-12_

---

## Deliverable Summary

This document is the output of PBI-5. It provides the prioritized implementation
sequence for the 8 demo use cases (PBIs 8–15), with dependency analysis, hardware
gap identification, and per-demo prerequisites.

---

## 1. Foundation — Must Complete Before Any Demo

These are not demos themselves but are gates that every demo depends on.

| Item | Owner | Status | Notes |
|------|-------|--------|-------|
| PBI-4: Rock-solid balance (≥99% reliability) | Firmware | 🔄 InProgress | Core blocker — no demo works on an unstable platform |
| PBI-1 task 1-3: USB serial Jetson ↔ Teensy | Firmware + SW | ✅ SW done | `#VEL` command + `JetsonBridge` written; needs hardware smoke-test |
| PBI-1 task 1-4: PS3 controller on Jetson | SW | Proposed | Jetson-side bridge not yet written |
| PBI-1 task 1-5: Mode blending + failsafes | Firmware + SW | Partial | Watchdog done; full mode-blend TBD |
| PBI-3: Tip-over auto-stop (±30°) | Firmware | ✅ Done | Firmware already stops motors at ±25° (tighter than spec) |
| `/odom` publisher in `balance_bridge` | ROS 2 | Proposed | Nav2 requires this; pure SW, no robot needed to write |
| ROS 2 `balance_bridge` colcon build | ROS 2 | ✅ Written | Needs `colcon build` smoke-test on Jetson |

---

## 2. Hardware Gap Analysis

| Hardware | In Inventory? | Needed By | Action |
|----------|--------------|-----------|--------|
| NVIDIA Jetson AGX Orin | ✅ Yes | All demos | — |
| Luxonis OAK-D Pro | ✅ Yes | PBIs 8, 9, 11, 13 | — |
| LDROBOT STL-19P/D500 lidar | ✅ Implied (package in repo) | PBIs 13, 16 | Confirm physical unit; run `lsusb` for CP2102 `10c4:ea60` |
| ReSpeaker microphone array | ❌ Not in inventory | PBIs 10, 14 | **Must purchase before starting audio demos** |
| PS3 controller | ❌ Not in inventory | PBI-1 task 1-4 | Purchase or substitute DS4 / Xbox controller |
| VR headset | ❌ Not in inventory | PBI-12 (optional path) | Skip VR; implement web-only teleoperation first |
| Robot arm / manipulator | ❌ Not in inventory | PBI-11 (manipulation path) | Implement tracking-only path; skip manipulation |

---

## 3. Dependency Graph

```
PBI-4 (balance) ─────────────────────────────────────┐
PBI-1 1-3/1-5 (serial + modes) ──────────────────────┤
balance_bridge /odom ────────────────────────────────┼──► ALL DEMOS
PBI-3 (tip-over, DONE) ──────────────────────────────┘

PBI-16 (lidar SLAM) ─────────────────────────────────┬──► PBI-8 (nav)
PBI-13 (sensor fusion) ──────────────────────────────┘    PBI-13

PBI-16 ──────────────────────────────────────────────────► PBI-13

OAK-D ROS 2 node ────────────────────────────────────┬──► PBI-9 (follow)
                                                      ├──► PBI-11 (objects)
                                                      └──► PBI-13 (fusion)

PBI-10 (ASR + LLM) ──────────────────────────────────────► PBI-14 (wake word)

PBI-12 (teleoperation) ──────── independent after foundation
PBI-15 (safety+recovery) ─────── independent (firmware extension)
PBI-7 (Isaac Sim) ──────────────── fully independent
```

---

## 4. Prioritized Implementation Sequence

### Wave 0 — Foundation (in progress, no demo deliverable)
Complete PBI-4, PBI-1 1-3/1-5, `/odom` publisher, `colcon build` smoke-test.
**Exit criterion**: robot balances reliably; `ros2 topic echo /imu/roll` shows live data.

---

### Wave 1 — Quick Wins (maximum value, minimum new hardware)

#### Demo Priority 1: PBI-15 — Safety + Recovery
**Why first**: Firmware is already 80% done (±25° auto-stop exists). Adds a compelling
"self-righting" behavior and improves safety for all subsequent demos. Zero new hardware.

**Prerequisites**: PBI-4 functional balance, BNO085 IMU working.

**Remaining work**:
- Firmware: add `FALLEN` state (|roll| > 45°) that locks motors and logs a `FALLEN` event
- Firmware: add tilt-warning zone (25°–45°) that reduces max current progressively
- ROS 2: publish `/balance/safety_state` (std_msgs/String: "OK" | "WARNING" | "FALLEN")
- Test: deliberately push robot; confirm motors cut, log records event, state topic updates

---

#### Demo Priority 2: PBI-12 — Remote Teleoperation (web)
**Why second**: Needs only `rosbridge_suite` + an OAK-D stream + the `balance_bridge`
node already written. Web browser is the "VR headset" — no special hardware. Enables
remote operation for all future testing and is the easiest show-stopping demo.

**Prerequisites**: Foundation (Wave 0), OAK-D ROS 2 node, `rosbridge_suite`.

**Work**:
- Install `ros-humble-rosbridge-suite` on Jetson
- Integrate OAK-D Pro with `depthai-ros` package
- Write simple HTML/JS joystick page that publishes `/cmd_vel` via `roslibjs`
- Stream compressed camera feed (`image_transport` JPEG) to web page
- Test: open laptop browser → drive robot from browser joystick

---

### Wave 2 — Navigation Infrastructure

#### Demo Priority 3: PBI-16 — SLAM (LDROBOT lidar)
**Why third**: `ldrobot_lidar_ros2` package already in repo and configured. SLAM is
foundational for PBI-8 and PBI-13. Should be done before investing in Nav2 config.

**Prerequisites**: Lidar hardware confirmed, TF chain (`map→odom→base_link→laser`),
`/odom` publisher working.

**Work**:
- Confirm lidar USB connection (`lsusb` for `10c4:ea60`)
- Add `base_link → laser` static TF to `balance_bridge.launch.py`
- Configure SLAM Toolbox for a narrow, slow-moving robot (low max velocity, high rotation
  weight — balancing robots move differently from ground robots)
- Record first map of the test area
- Test: `rviz2` shows map building in real time

---

#### Demo Priority 4: PBI-7 — Isaac Sim / URDF (can parallelize with Wave 2)
**Why here**: Fully offline — can be worked on while waiting for robot availability.
URDF enables algorithm testing in simulation, reducing hardware time for PBIs 8–9.

**Prerequisites**: Isaac Sim installed on development machine (not Jetson).

**Work**:
- Create URDF with known dimensions (6.5″ hoverboard wheels, 165 mm diameter, ~400 mm
  wheelbase, estimated mass distribution from component list)
- Add BNO085 IMU link, OAK-D camera link, lidar link
- Isaac Sim launch config with differential drive plugin
- Validate: simulated robot balances with same PID gains as real robot (qualitative check)

---

### Wave 3 — Perception + Autonomy

#### Demo Priority 5: PBI-13 — Multi-Sensor Fusion
**Why fifth**: Brings together the lidar (PBI-16) and OAK-D; improves localization
accuracy. Natural stepping stone to PBI-8 autonomous navigation.

**Prerequisites**: PBI-16 (lidar SLAM working), OAK-D ROS 2 node, `/odom`.

**Work**:
- Integrate `depthai-ros` for OAK-D visual odometry (`/stereo/odometry`)
- Fuse lidar SLAM pose + visual odometry + IMU using `robot_localization` EKF node
- Publish unified `/odom` fused from all three sources
- Replace single-source `/odom` in `balance_bridge` with fused estimate

---

#### Demo Priority 6: PBI-8 — Vision-Driven Autonomy (Nav2)
**Why sixth**: The flagship demo. Depends on reliable SLAM (PBI-16) and good odometry
(PBI-13). Nav2 stack will not work well on a ±5% odometry estimate.

**Prerequisites**: PBI-13, SLAM map, Nav2 installed.

**Work**:
- Configure Nav2 for differential drive with the robot's specific kinematic limits
  (very slow max speed ~0.5 m/s; cannot reverse quickly; tipping risk)
- Custom Nav2 progress monitor that pauses navigation if `safety_state` ≠ "OK"
- Record a map → set waypoints → autonomous navigation demo
- Test: navigate a 5 m × 5 m loop three times without manual intervention

---

#### Demo Priority 7: PBI-9 — Human Following + Gesture
**Why seventh**: OAK-D is already integrated (PBI-13); person detection is an
incremental add. Lower complexity than full Nav2.

**Prerequisites**: PBI-13 (OAK-D node), balance reliable.

**Work**:
- Run MobileNet-SSD person detector on OAK-D (runs on OAK's on-chip MyriadX)
- Implement follow controller: maintain 1.0–1.5 m distance, turn to keep person centered
- Implement 3 gesture commands (stop, follow me, spin) via hand pose classifier
- Test: follow a person across a room; respond to gestures

---

### Wave 4 — Audio Demos (hardware purchase required)

**Gate**: Purchase ReSpeaker USB mic array before starting Wave 4.
**Estimated cost**: ~$60–80 (ReSpeaker 4-mic or 6-mic USB array).

#### Demo Priority 8: PBI-10 — Conversational Companion (ASR + LLM)
**Prerequisites**: ReSpeaker purchased and connected, Whisper installed on Jetson,
local LLM (recommend `llama.cpp` with Llama-3.2-3B — fits in Jetson AGX Orin's RAM).

**Work**:
- Integrate ReSpeaker with ROS 2 (`audio_common` or direct ALSA)
- Run Whisper `small.en` model for real-time STT (Jetson GPU)
- Run `llama.cpp` server with a 3B parameter model for response generation
- Implement conversation loop: listen → STT → LLM → TTS (`piper` or `espeak`)
- Voice control commands: "go forward", "stop", "turn left/right", "follow me"
- Test: 5-turn conversation; 3 robot control commands via voice

---

#### Demo Priority 9 (final): PBI-14 — Voice Command + Wake Word
**Prerequisites**: PBI-10 complete, `openWakeWord` or `Porcupine` library.

**Work**:
- Implement always-listening wake word ("Hey Johnny" or similar)
- On wake: activate Whisper STT for 5-second command window
- Idle mode: motors off, Jetson low-power, only wake word engine running
- Test: wake from 3 m away; 90%+ wake word detection; 0 false positives per minute

---

#### Demo Priority 10: PBI-11 — Object Recognition (tracking-only path)
**Note**: No manipulator in inventory. Implement as "identify + approach" not "pick up".

**Prerequisites**: OAK-D integrated (PBI-13).

**Work**:
- Run YOLOv5/v8-nano on OAK-D MyriadX for real-time detection
- Implement "approach and announce" behavior: detect object → drive to 0.5 m → announce
- Test: identify 5 common objects (bottle, chair, person, box, phone) at 90%+ accuracy

---

## 5. Summary Table

| Wave | PBI | Demo | Hardware Gap | Effort Est. | Value |
|------|-----|------|-------------|-------------|-------|
| 0 | Foundation | Balance + serial + /odom | None | — | Blocker |
| 1 | PBI-15 | Safety + Recovery | None | Small | High |
| 1 | PBI-12 | Remote Teleoperation | None | Medium | High |
| 2 | PBI-16 | SLAM (lidar) | Confirm lidar unit | Medium | High |
| 2 | PBI-7 | Isaac Sim URDF | None (offline) | Medium | Medium |
| 3 | PBI-13 | Multi-Sensor Fusion | None | Medium | High |
| 3 | PBI-8 | Vision-Driven Autonomy | None | Large | Very High |
| 3 | PBI-9 | Human Following | None | Medium | High |
| 4 | PBI-10 | Conversational Companion | **Buy ReSpeaker** | Large | High |
| 4 | PBI-14 | Wake Word | ReSpeaker (from PBI-10) | Small | Medium |
| 4 | PBI-11 | Object Recognition | None | Medium | Medium |

---

## 6. Decisions Logged

| Decision | Rationale |
|----------|-----------|
| PBI-12 teleoperation uses web-only (no VR) | VR headset not in inventory; web browser reaches the same demo audience |
| PBI-11 uses tracking-only (no manipulation) | No arm/manipulator in inventory; no plan to add one |
| Audio demos (PBIs 10, 14) deferred to Wave 4 | ReSpeaker not in inventory; all other demos have zero additional hardware cost |
| PBI-7 (Isaac Sim) placed in Wave 2 | Fully offline; enables algorithm development for later waves |
| PBI-16 before PBI-13 before PBI-8 | Clear dependency chain; skipping steps leads to poor Nav2 performance |
