# PBI-13: Demo 6 - Multi-Sensor Fusion (IMU + Camera + Lidar)

## Overview
Implement Demo 6: Multi-Sensor Fusion so the robot can combine data from IMU, camera, and lidar for robust perception and localization.

## Problem Statement
Individual sensors have limitations. Fusing IMU, camera, and lidar data provides more robust and accurate perception and localization than any single sensor.

## User Stories
- As a developer, I want sensor fusion, so that perception is robust to individual sensor failures.
- As a developer, I want fused localization, so that robot position is accurate and reliable.

## Technical Approach
- Integrate lidar sensor (RPLidar or similar) with ROS 2.
- Implement sensor fusion framework combining IMU, camera, and lidar data.
- Create unified perception pipeline with fused sensor data.
- Test fusion accuracy and robustness compared to individual sensors.
- Document sensor fusion architecture and calibration procedures.

## UX/UI Considerations
- Fusion visualization: display of fused sensor data.
- Sensor status: indication of which sensors are active.

## Acceptance Criteria
- Lidar sensor integrated with ROS 2.
- Sensor fusion framework combines IMU, camera, and lidar data.
- Unified perception pipeline uses fused data.
- Fusion accuracy better than individual sensors.
- Robustness validated (handles sensor failures gracefully).
- All CoS from backlog met.

## Physical Robot Changes
- **Lidar sensor mounting**: Mount lidar sensor (RPLidar or similar) on robot.
  - Mounting height: 6-8 inches (15-20 cm) above ground per `robotics_design_methodology.md`.
  - Mounting orientation: Horizontal, parallel to floor (0-2° tilt).
  - Mounting options:
    - **Option A (Recommended)**: Dedicated micro-deck between top deck and mast (25-50mm standoffs).
    - **Option B**: Mast mount with forward-offset arm (50-70mm) to avoid mast occlusion.
  - Cable routing: Power and data cables from lidar to Jetson (ensure secure routing).
  - Power: Lidar power requirements (typically 5V or 12V, check sensor specs).
- **No wiring changes to Teensy** - lidar communicates with Jetson.
- **Physical testing**: Requires robot to move in physical space for sensor fusion validation.

## Dependencies
- Lidar sensor hardware (RPLidar or similar).
- PBI 8 (Vision-Driven Autonomy) - camera integration.
- PBI 1 (Teensy Core Firmware) - IMU data source.
- PBI 6 (Aluminum Chassis Build) - lidar mounting depends on chassis design.

## Open Questions
- Which lidar sensor: RPLidar vs other options?
- Fusion algorithm: Kalman filter vs particle filter vs other?
- Lidar mounting option: micro-deck vs mast mount (affects field of view and occlusion).

## Related Tasks
See [Tasks for PBI 13](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


