# Robotics Project Overview
_Last updated: 2025-09-03_

## 1. Project Goals
- Develop a **carry-on sized, two-wheeled self-balancing robot** capable of ~8 mph.
- Use **NVIDIA Jetson AGX Orin Dev Kit** for onboard perception, planning, and interaction.
- Demonstrate **8 core demos**:
  1. Vision-Driven Autonomy (visual SLAM + Nav2).
  2. Human Following + Gesture Control.
  3. Conversational Companion (local ASR + LLM).
  4. Object Recognition + Manipulation.
  5. Remote Teleoperation (web/VR).
  6. Multi-Sensor Fusion (IMU + camera + lidar).
  7. Voice Command + Wake Word.
  8. Safety + Recovery (fall detection, auto-balance restore).

## 2. Physical Design
- **Chassis**: plate + standoff construction.
  - Bottom base plate: **3 mm aluminum** (rigid, crash-resistant).
  - Top plate: **6 mm Lexan** or **3 mm aluminum** (to be finalized).
- **Mast**: LS2015 carbon fiber survey pole (modular, 1.8 m, detachable).
- **Deck stacking**:
  - Lower deck: batteries + FSESC.
  - Middle deck: Jetson + power distribution.
  - Upper deck: Teensy, IMU, sensors.
- **Ground clearance**: 2–3 in with hoverboard hubs.

## 3. Hardware Summary (Specs, not costs)
- **Compute**: NVIDIA Jetson AGX Orin Dev Kit.
- **Motors**: 2× Gyroor 6.5″ hoverboard hub motors.
- **Motor Controller**: Flipsky Dual Mini FSESC6.7 Pro.
- **Power**:
  - 2× HRB 5S 5000 mAh LiPo (wired in series, 10S).
  - IPS-DTD24S198 buck converter (24→19 V @ 8 A for Jetson).
  - Bus bars, battery switch, fuses, XT90-S anti-spark connectors.
- **Microelectronics**:
  - PJRC Teensy 4.1.
  - Treedix breakout board.
  - BNO085 9DOF IMU.
- **Sensors**:
  - Luxonis OAK-D Pro (stereo/depth).
  - ReSpeaker microphone array (for ASR).
  - Optional: RPLidar / ToF rangefinder.
- **Mechanical**:
  - Aluminum motor mounts, wheel brackets.
  - Aluminum frame kit (to be integrated with plates).

## 4. Software Stack
- **Base OS**: Ubuntu (Jetson).
- **Framework**: ROS 2 (control, Nav2, SLAM).
- **Vision**: DepthAI + OpenCV for OAK-D.
- **Audio**: Whisper ASR + local LLM integration.
- **Embedded**: Teensy firmware (Arduino/PlatformIO), optional micro-ROS.
- **Dev Tools**: Onshape (CAD), Cursor (coding), Zoo.ai (layout).

## 5. Key Design Considerations
- **Airline Compliance**: batteries < 100 Wh each, modular mast detaches.
- **Balance & Stability**: weight centered over wheel axis.
- **Thermals**: Jetson + buck require airflow/heatsinking.
- **Safety**: anti-spark connectors, main cutoff switch, fused branches.
- **Future Expansion**: scalable to DGX Spark for offline training.

---

This overview is a concise context file. For detailed BOM and cost tracking, see `robotics_inventory_costs.md`.