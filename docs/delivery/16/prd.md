# PBI-16: SLAM (Simultaneous Localization and Mapping)

## Overview
Implement lidar-based SLAM (Simultaneous Localization and Mapping) using LDROBOT STL-19P/D500 lidar sensor and SLAM Toolbox, so the robot can build maps of its environment and localize itself within those maps for autonomous navigation.

## Problem Statement
The robot needs to understand its environment and know its position within that environment to enable autonomous navigation, path planning, and higher-level autonomy features. SLAM provides both mapping (creating a map) and localization (knowing position in the map) simultaneously.

## User Stories
- As a developer, I want the robot to build maps of its environment, so that it can navigate autonomously.
- As a developer, I want the robot to localize itself within maps, so that it knows where it is at all times.
- As a user, I want the robot to create accurate maps, so that navigation is reliable and safe.

## Technical Approach
- Integrate LDROBOT STL-19P/D500 lidar sensor with ROS 2.
- Install and configure SLAM Toolbox for ROS 2.
- Set up lidar driver (ldlidar_stl_ros2) with proper parameters.
- Configure TF chain: map → odom → base_link → laser.
- Implement SLAM Toolbox with appropriate parameters for balance robot dynamics.
- Create launch files for mapping and localization modes.
- Integrate with robot's odometry (from Teensy IMU/encoder data).
- Test mapping in various environments (indoor, outdoor, different lighting).
- Test localization accuracy and loop closure.
- Document SLAM configuration and tuning parameters.

## UX/UI Considerations
- Map visualization: Real-time display of SLAM map in RViz.
- Localization feedback: Visual indication of robot pose and confidence.
- Mapping status: Clear indication of mapping vs localization mode.

## Acceptance Criteria
- LDROBOT lidar sensor integrated with ROS 2 and publishing scan data.
- SLAM Toolbox running and creating maps successfully.
- Maps are accurate and usable for navigation (>90% accuracy).
- Localization works reliably in mapped environments (>95% success rate).
- Loop closure detection working correctly.
- TF chain properly configured and validated.
- Mapping tested in multiple environments (indoor, outdoor, different sizes).
- Localization tested with map loading and re-localization.
- All CoS from backlog met.

## Physical Robot Changes
- **LDROBOT STL-19P/D500 lidar mounting**: Mount lidar sensor on robot per `robotics_design_methodology.md` specifications.
  - **Mounting height**: 6-8 inches (15-20 cm) above ground, parallel to floor (0-2° tilt).
  - **Mounting options**:
    - **Option A (Recommended)**: Dedicated micro-deck between top deck and mast plate (25-50mm standoffs).
      - Center lidar on plate with 360° clearance.
      - Route cables downward through plate.
      - Place mast clamp/plate behind lidar; keep mast center ≥50-70mm behind lidar center.
      - Note: 32mm mast behind sensor creates rear blind sector (~37° at 50mm, ~18° at 100mm). Mask this sector in mapping as needed.
    - **Option B**: Mast mount with forward-offset arm (50-70mm) to avoid mast occlusion.
      - Maintain co-location with LD500 kit's sensor (within 1-4 inches).
      - Ensure rigid and short relative transform.
      - Accept small rear blind sector and account for it in SLAM/localization.
  - **Cable routing**: USB-UART bridge cable from lidar to Jetson (ensure secure routing, avoid interference).
  - **Power**: Lidar powered via USB from Jetson (CP2102 USB-UART bridge).
  - **Orientation**: Horizontal, parallel to floor (0-2° tilt maximum).
  - **Clearance**: Ensure 360° scan plane clearance (minimize obstructions in scan plane).
- **No wiring changes to Teensy** - lidar communicates directly with Jetson via USB.
- **Physical testing**: Requires robot to move in physical space for mapping and localization testing.

## Dependencies
- PBI 1 (Teensy Core Firmware) - requires working robot platform and odometry data.
- LDROBOT STL-19P/D500 lidar hardware.
- ROS 2 installed on Jetson.
- SLAM Toolbox ROS 2 package.
- PBI 6 (Aluminum Chassis Build) - lidar mounting depends on chassis design.
- Odometry source: IMU data from Teensy (via USB serial) or wheel encoders.

## Open Questions
- Odometry source: Use IMU-based odometry from Teensy or wheel encoder odometry?
- SLAM Toolbox parameters: Tuning for balance robot dynamics (may need custom parameters).
- Map storage: Where to store maps (Jetson filesystem, persistent storage)?
- Loop closure tuning: Parameters for reliable loop closure detection.
- Real-time vs offline mapping: Should mapping run in real-time or post-process scans?

## Related Tasks
See [Tasks for PBI 16](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)

## Technical Notes
- **Lidar Specifications**:
  - Model: LDROBOT STL-19P/D500 Kit
  - USB-UART Bridge: CP2102 (VID:PID 10c4:ea60)
  - Baud Rate: 230400 (8N1)
  - Scan Rate: 10 Hz
  - Points per scan: 502 ranges
  - Range: 0.02m to 25m
  - Angular Resolution: ~0.7° (502 points/360°)
  - Range Accuracy: ±2cm

- **SLAM Toolbox Configuration**:
  - Base frame: `base_link`
  - Odometry frame: `odom`
  - Map frame: `map`
  - Scan topic: `/scan`
  - Mode: Mapping (for initial map creation), Localization (for using existing maps)

- **TF Chain Requirements**:
  - `map` → `odom` (published by SLAM Toolbox)
  - `odom` → `base_link` (from robot odometry)
  - `base_link` → `laser` (static transform from lidar mounting)

- **Integration with Existing Code**:
  - Reference: `ldrobot_lidar_ros2/` directory contains existing lidar integration setup.
  - Launch files: `complete_lidar_slam.launch.py`, `lidar_only.launch.py`, `slam_only.launch.py`.
  - Configuration: `lidar_params.yaml`, `slam_params.yaml`.
  - Setup script: `scripts/setup_lidar.sh`.

