# PBI-8: Demo 1 - Vision-Driven Autonomy (Visual SLAM + Nav2)

## Overview
Implement Demo 1: Vision-Driven Autonomy using visual SLAM and Nav2 so the robot can autonomously navigate using camera-based perception and mapping.

## Problem Statement
The robot needs autonomous navigation capability using visual perception. This is a foundational demo that enables other autonomy features.

## User Stories
- As a developer, I want visual SLAM, so that the robot can build maps using camera data.
- As a developer, I want Nav2 integration, so that the robot can navigate autonomously to waypoints.

## Technical Approach
- Integrate OAK-D Pro camera with ROS 2.
- Implement visual SLAM using SLAM toolbox or similar (RTAB-Map, ORB-SLAM3).
- Configure Nav2 for autonomous navigation.
- Test SLAM mapping in controlled environment.
- Test autonomous navigation to waypoints.
- Document SLAM configuration and navigation parameters.

## UX/UI Considerations
- Map visualization: display of SLAM map and robot pose.
- Navigation feedback: status of navigation goals and path planning.

## Acceptance Criteria
- OAK-D Pro integrated with ROS 2.
- Visual SLAM creates accurate maps.
- Nav2 navigates robot to waypoints successfully.
- Mapping tested in controlled environment.
- Navigation tested with multiple waypoints.
- All CoS from backlog met.

## Physical Robot Changes
- **OAK-D Pro camera mounting**: Mount OAK-D Pro camera on robot (typically on mast or upper deck).
  - Mounting height: Per design specifications (see `robotics_design_methodology.md`).
  - Mounting orientation: Forward-facing for navigation.
  - Cable routing: USB-C cable from camera to Jetson (ensure secure routing).
  - Power: Camera powered via USB-C from Jetson.
- **No wiring changes to Teensy** - camera communicates directly with Jetson.
- **Physical testing**: Requires robot to move in physical space for SLAM mapping and navigation testing.

## Dependencies
- PBI 1 (Teensy Core Firmware) - requires working robot platform.
- OAK-D Pro camera hardware.
- ROS 2 installed on Jetson.
- PBI 5 (Use Case Prioritization) - may affect implementation order.
- PBI 6 (Aluminum Chassis Build) - camera mounting may depend on chassis design.

## Open Questions
- Which SLAM algorithm: RTAB-Map vs ORB-SLAM3 vs SLAM toolbox?
- Navigation parameters: tuning for balance robot dynamics.
- Camera mounting location: mast vs upper deck (affects field of view and stability).

## Related Tasks
See [Tasks for PBI 8](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


