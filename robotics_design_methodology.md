# Robotics Design Methodology
_Last updated: 2025-09-03_

## 1. Guiding Principles
- **Compact and Portable**: Robot must fit inside a carry-on (Travelpro) for transport.
- **Balance First**: All design choices checked against stability and CoG alignment.
- **Modularity**: Plates, standoffs, and mast allow rapid iteration and part swapping.
- **Safety and Reliability**: Proper fusing, anti-spark connectors, and branch isolation.
- **Scalability**: Start lightweight, but leave hooks for advanced features (e.g. DGX Spark training).

## 2. Mechanical Decisions
- **Bottom Plate (3 mm Aluminum)**:
  - Chosen for stiffness, durability, and heat spreading.
  - Provides solid mounting points for heavy components (batteries, FSESC).
- **Top Plate (6 mm Lexan vs 3 mm Aluminum)**:
  - Lexan: impact resistance, optical clarity, easy sensor mounting.
  - Aluminum: lighter for stiffness, better grounding path.
  - Decision deferred until prototype weight balance tested.
- **Standoff Construction**:
  - Lightweight, easier to fit than 2020 extrusion.
  - Allows clear cable routing and modular decks.
- **Mast**:
  - LS2015 carbon fiber survey pole, detachable for travel.
  - Provides height for OAK-D camera without heavy frame extensions.
  - 32 mm diameter; segmented: 3 × 0.45 m sections + 1 × 0.37 m section.
  - Target overall system height ≈ 60 in using partial mast (not full 1.72 m).
  - Preferred configs for stiffness/portability:
    - 3 × 0.45 m = 1.35 m (~53 in) plus deck/wheel height.
    - 0.45 + 0.45 + 0.37 m = 1.27 m (~50 in) when lower CoG is desired.
  - Keep unused section(s) detached to meet venue/transport constraints.

### LIDAR Mounting Strategy
- **Height Target**: Mount planar LIDAR at 6–8 in (15–20 cm) above ground, parallel to floor (0–2° tilt).
- **Option A — Dedicated micro-deck (recommended for 360°)**:
  - Add a small LIDAR-only plate between top deck and mast plate (25–50 mm standoffs).
  - Center the LIDAR on the plate with 360° clearance; route cables downward through the plate.
  - Place the mast clamp/plate behind the LIDAR; keep mast center ≥ 50–70 mm behind LIDAR center.
  - Note: A 32 mm mast behind the sensor creates a rear blind sector. If mast center is ~50 mm behind the LIDAR center, the occlusion is ~37°; at ~100 mm, ~18°. Mask this sector in mapping as needed.
- **Option B — Mast mount (for co-location constraints)**:
  - Use a 32 mm mast clamp and a forward-offset arm (50–70 mm) so the mast does not intersect the LIDAR scan plane.
  - Maintain required co-location with the LD500 kit’s sensor (keep within 1–4 in), ensuring their relative transform is rigid and short.
  - Ensure unobstructed forward/side views; accept a small rear blind sector and account for it in SLAM/localization.
  - Keep tilt within 0–2° and avoid placing reflective hardware in the scan plane.


## 3. Electrical and Wiring Methodology
- **Power Topology**:
  - 2× 5S LiPo in series → 10S ~37 V bus.
  - ESCs draw directly from 10S.
  - Jetson powered via buck converter (24→19 V @ 8 A).
  - Teensy + sensors powered via regulated 5 V branch.
- **Wiring Practices**:
  - Use appropriate gauge (10–16 AWG) depending on current path.
  - Crimped and heat-shrink sealed connections (Twidec terminals).
  - Separate logic vs power wiring to reduce noise.
- **Fusing Strategy**:
  - Main cutoff switch (Nilight) + XT90-S anti-spark connectors.
  - 60 A fuse inline with ESC branch.
  - 10 A fuse for Jetson branch.
  - Smaller fuses for auxiliaries as needed.
- **Control Bus**:
  - CAN bus linking Jetson ↔ Teensy ↔ FSESC.
  - Hall sensors wired directly to ESC (not MCU).

## 4. Software Methodology
- **Jetson Stack**:
  - ROS 2 core: Nav2 for navigation, SLAM toolbox for localization.
  - Perception via DepthAI (OAK-D), OpenCV.
  - Audio via Whisper ASR + local LLM (for commands/conversation).
- **Teensy Stack**:
  - Real-time IMU data acquisition (BNO085).
  - Bridge between Jetson and FSESC via CAN.
  - Optional: micro-ROS for tighter ROS integration.
- **Development Flow**:
  - Start with basic balance controller on Teensy + ESC.
  - Add Jetson for SLAM/vision autonomy.
  - Integrate voice + LLM modules after stable locomotion achieved.

## 5. Iteration Strategy
- **Phase 1**: Mechanical mock-up with plates, mounts, and wiring dry-run.
- **Phase 2**: Balance testing with Teensy + ESC only.
- **Phase 3**: Add Jetson, vision, and SLAM.
- **Phase 4**: Integrate autonomy demos (human-follow, voice control).
- **Phase 5**: Safety validation (e-stop, recovery routines).

---

This file defines the decision-making framework for mechanical, electrical, and software design choices in the robotics project.