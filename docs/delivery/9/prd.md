# PBI-9: Demo 2 - Human Following + Gesture Control

## Overview
Implement Demo 2: Human Following + Gesture Control so the robot can detect and follow a person while responding to gestures.

## Problem Statement
The robot should be able to follow a person autonomously and respond to hand gestures for intuitive interaction.

## User Stories
- As a user, I want the robot to follow me, so that it maintains a safe distance and avoids obstacles.
- As a user, I want to control the robot with gestures, so that I can interact without a controller.

## Technical Approach
- Implement person detection using OAK-D Pro depth camera.
- Create human following algorithm (maintain distance, avoid obstacles).
- Implement gesture recognition (hand gestures for commands).
- Integrate with balance controller for smooth following motion.
- Test following behavior in various scenarios.
- Document gesture vocabulary and following parameters.

## UX/UI Considerations
- Gesture feedback: visual confirmation when gesture recognized.
- Following status: indication of following mode and target person.

## Acceptance Criteria
- Person detection working reliably.
- Human following maintains safe distance.
- Gesture recognition recognizes defined gestures.
- Following integrates smoothly with balance controller.
- Tested in various scenarios (indoor, outdoor, different lighting).
- All CoS from backlog met.

## Physical Robot Changes
- **OAK-D Pro camera**: Uses same camera mounting as PBI 8 (Vision-Driven Autonomy).
  - If PBI 8 not completed: Mount OAK-D Pro camera per PBI 8 specifications.
  - Camera orientation: Forward-facing for person detection and gesture recognition.
- **No additional wiring changes** - camera communicates with Jetson via USB-C.
- **Physical testing**: Requires robot to move and follow person in physical space.

## Dependencies
- PBI 8 (Vision-Driven Autonomy) - may share camera/perception infrastructure.
- OAK-D Pro camera hardware.
- PBI 1 (Teensy Core Firmware) - requires working balance controller.

## Open Questions
- Gesture vocabulary: which gestures to support?
- Following distance: optimal distance for safety and visibility.

## Related Tasks
See [Tasks for PBI 9](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


