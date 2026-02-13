# PBI-5 Tasks: Demo Use Case Prioritization
_Status: Done | Completed: 2026-02-12 | Updated: 2026-02-13 (ReSpeaker + ESP32 purchased)_

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
| PBI-1 task 1-4: PS3 controller via ESP32 | ESP32 FW + ROS 2 | ✅ SW done | `esp32/ps3_bridge/ps3_bridge.ino` + `esp32_joy_node.py` written; needs hardware pairing |
| PBI-1 task 1-5: Mode blending + failsafes | Firmware + SW | Partial | Watchdog done; `twist_mux` priority arbitration written; full mode-blend TBD |
| PBI-3: Tip-over auto-stop (±30°) | Firmware | ✅ Done | Firmware already stops motors at ±25° (tighter than spec) |
| `/odom` publisher in `balance_bridge` | ROS 2 | ✅ Done | Dead-reckoning odom + TF broadcaster added to `balance_bridge_node.py` |
| ROS 2 `balance_bridge` colcon build | ROS 2 | ✅ Written | Needs `colcon build` smoke-test on Jetson |
| `twist_mux` config | ROS 2 | ✅ Done | `config/twist_mux.yaml` written; joy(100)>nav(50)>web(25)>voice(10) |

---

## 2. Hardware Gap Analysis

| Hardware | In Inventory? | Needed By | Action |
|----------|--------------|-----------|--------|
| NVIDIA Jetson AGX Orin | ✅ Yes | All demos | — |
| Luxonis OAK-D Pro | ✅ Yes | PBIs 8, 9, 11, 13 | — |
| LDROBOT STL-19P/D500 lidar | ✅ Implied (package in repo) | PBIs 13, 16 | Confirm physical unit; run `lsusb` for CP2102 `10c4:ea60` |
| ReSpeaker microphone array | ✅ Purchased | PBIs 10, 14 | Hardware gap closed — audio demos unblocked |
| ESP32 (PS3-compatible) | ✅ Purchased | PBI-1 task 1-4 | Bridges PS3 BT → Jetson USB; use `ps3Controller` Arduino lib |
| PS3 controller | — | PBI-1 task 1-4 | Paired via ESP32 — no direct Jetson BT pairing needed |
| VR headset | ❌ Not in inventory | PBI-12 (optional path) | Skip VR; implement web-only teleoperation first |
| Robot arm / manipulator | ❌ Not in inventory | PBI-11 (manipulation path) | Implement tracking-only path; skip manipulation |

---

## 3. Dependency Graph

```
PBI-4 (balance) ─────────────────────────────────────┐
PBI-1 1-3/1-5 (serial + modes) ──────────────────────┤
balance_bridge /odom ────────────────────────────────┼──► ALL DEMOS
PBI-3 (tip-over, DONE) ──────────────────────────────┘

ESP32 (PS3 BT) ──► joy_node ──► teleop_twist_joy ─────┐
Nav2 ─────────────────────────────────────────────────┼──► twist_mux ──► /cmd_vel ──► balance_bridge
Web joystick (PBI-12) ────────────────────────────────┘

PBI-16 (lidar SLAM) ─────────────────────────────────┬──► PBI-8 (nav)
                                                      └──► PBI-13 (sensor fusion)

OAK-D ROS 2 node ────────────────────────────────────┬──► PBI-9 (follow)
                                                      ├──► PBI-11 (objects)
                                                      └──► PBI-13 (fusion)

ReSpeaker ──► PBI-10 (ASR + LLM) ───────────────────────► PBI-14 (wake word)

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

#### Foundation addition: ESP32 PS3 bridge + `/cmd_vel` mux
**New work unlocked by ESP32 purchase.** Before physical joystick control can work
alongside Nav2, a `/cmd_vel` mux must arbitrate between sources.

**Work**:
- Flash ESP32 with `ps3Controller` Arduino library; emit joystick axes as newline-delimited
  JSON over USB serial (e.g. `{"lx":0.5,"ly":0.0,"rx":-0.2}`)
- Write thin ROS 2 node `esp32_joy_node` that reads the serial JSON and publishes
  `sensor_msgs/Joy` on `/joy`
- Install `teleop_twist_joy` + configure axes mapping (linear.x = left stick Y,
  angular.z = right stick X)
- Install `twist_mux` with priority ordering:
  1. `/cmd_vel_joy` (highest — manual always wins)
  2. `/cmd_vel_nav` (Nav2 output)
  3. `/cmd_vel_web` (web joystick from PBI-12)
- All three merge into `/cmd_vel` consumed by `balance_bridge`
- Test: joystick overrides Nav2 mid-run; releasing stick hands back to Nav2

---

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

### Wave 2 — Navigation Infrastructure + Audio (parallel tracks)

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

#### Demo Priority 4 (parallel track A): PBI-10 — Conversational Companion (ASR + LLM)
**Moved up from Wave 4** — ReSpeaker now in inventory; zero dependency on SLAM or OAK-D.
Can be developed fully in parallel with navigation work.

**Prerequisites**: ReSpeaker connected, Jetson internet access for model download.

**Work**:
- Connect ReSpeaker; verify with `arecord -l` (expect UAC1.0 device)
- Install Whisper (`openai-whisper`, run `small.en` on Jetson GPU)
- Install `llama.cpp` server; pull `Llama-3.2-3B-Instruct.Q4_K_M.gguf` (~2 GB, fits in
  Jetson AGX Orin's 32 GB unified RAM)
- Write `voice_bridge` ROS 2 node: ReSpeaker → Whisper STT → LLM → TTS (`piper`) →
  speaker; publishes recognized commands to `/voice/command` (std_msgs/String)
- Write `voice_cmd_vel` node: subscribes `/voice/command`, emits to `/cmd_vel_voice`
  (fed into `twist_mux` at lowest priority — voice < web < joy)
- Test: "go forward", "stop", "turn left", "follow me" recognized correctly

---

#### Demo Priority 5 (parallel track B): PBI-7 — Isaac Sim / URDF (can parallelize with Wave 2)
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

#### Demo Priority 6: PBI-14 — Voice Command + Wake Word
**Moved up from Wave 4** — PBI-10 now in Wave 2; wake word is a direct extension of
the same `voice_bridge` node.

**Prerequisites**: PBI-10 complete.

**Work**:
- Add always-listening wake word using `openWakeWord` (open-source, runs on CPU)
- Keyword: "Hey Johnny" (or similar; can be custom-trained)
- On detection: activate 5-second Whisper STT window; suppress otherwise
- Add low-power idle mode: balance controller in standby, only wake engine running
- Test: wake from 3 m away; ≥90% detection rate; ≤1 false positive per minute

---

#### Demo Priority 7: PBI-13 — Multi-Sensor Fusion
**Why here**: Brings together the lidar (PBI-16) and OAK-D; improves localization
accuracy. Natural stepping stone to PBI-8 autonomous navigation.

**Prerequisites**: PBI-16 (lidar SLAM working), OAK-D ROS 2 node, `/odom`.

**Work**:
- Integrate `depthai-ros` for OAK-D visual odometry (`/stereo/odometry`)
- Fuse lidar SLAM pose + visual odometry + IMU using `robot_localization` EKF node
- Publish unified `/odom` fused from all three sources
- Replace single-source `/odom` in `balance_bridge` with fused estimate

---

#### Demo Priority 9: PBI-8 — Vision-Driven Autonomy (Nav2)
**Why here**: The flagship demo. Depends on reliable SLAM (PBI-16) and good odometry
(PBI-13). Nav2 stack will not work well on a ±5% odometry estimate.

**Prerequisites**: PBI-13, SLAM map, Nav2 installed.

**Work**:
- Configure Nav2 for differential drive with the robot's specific kinematic limits
  (very slow max speed ~0.5 m/s; cannot reverse quickly; tipping risk)
- Custom Nav2 progress monitor that pauses navigation if `safety_state` ≠ "OK"
- Record a map → set waypoints → autonomous navigation demo
- Test: navigate a 5 m × 5 m loop three times without manual intervention

---

#### Demo Priority 10: PBI-9 — Human Following + Gesture
**Why here**: OAK-D is already integrated (PBI-13); person detection is an
incremental add. Lower complexity than full Nav2.

**Prerequisites**: PBI-13 (OAK-D node), balance reliable.

**Work**:
- Run MobileNet-SSD person detector on OAK-D (runs on OAK's on-chip MyriadX)
- Implement follow controller: maintain 1.0–1.5 m distance, turn to keep person centered
- Implement 3 gesture commands (stop, follow me, spin) via hand pose classifier
- Test: follow a person across a room; respond to gestures

---

### Wave 4 — Object Recognition

#### Demo Priority 11: PBI-11 — Object Recognition (tracking-only path)
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
| 0 | Foundation | Balance + serial + /odom + ESP32 mux | None | — | Blocker |
| 1 | PBI-15 | Safety + Recovery | None | Small | High |
| 1 | PBI-12 | Remote Teleoperation | None | Medium | High |
| 2A | PBI-16 | SLAM (lidar) | Confirm lidar unit | Medium | High |
| 2A | PBI-7 | Isaac Sim URDF | None (offline) | Medium | Medium |
| 2B | PBI-10 | Conversational Companion | ~~Buy ReSpeaker~~ ✅ | Large | High |
| 3 | PBI-14 | Wake Word | ReSpeaker (from PBI-10) | Small | Medium |
| 3 | PBI-13 | Multi-Sensor Fusion | None | Medium | High |
| 3 | PBI-8 | Vision-Driven Autonomy | None | Large | Very High |
| 3 | PBI-9 | Human Following | None | Medium | High |
| 4 | PBI-11 | Object Recognition | None | Medium | Medium |

**All hardware now in inventory. Zero remaining purchase gates.**

---

## 6. Decisions Logged

| Decision | Rationale |
|----------|-----------|
| PBI-12 teleoperation uses web-only (no VR) | VR headset not in inventory; web browser reaches the same demo audience |
| PBI-11 uses tracking-only (no manipulation) | No arm/manipulator in inventory; no plan to add one |
| PBI-10 + PBI-14 moved from Wave 4 → Waves 2B/3 | ReSpeaker purchased 2026-02-13; audio demos no longer hardware-gated |
| PS3 via ESP32, not Jetson BT | ESP32 purchased 2026-02-13; use `ps3Controller` Arduino lib + standard `joy_node` + `teleop_twist_joy`; no custom driver needed |
| `/cmd_vel` mux added to foundation | ESP32 joystick + Nav2 + web all publish `/cmd_vel`; `twist_mux` priority: joy > nav > web > voice |
| PBI-7 (Isaac Sim) placed in Wave 2A | Fully offline; enables algorithm development for later waves |
| PBI-16 before PBI-13 before PBI-8 | Clear dependency chain; skipping steps leads to poor Nav2 performance |
