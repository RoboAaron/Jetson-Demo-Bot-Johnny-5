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