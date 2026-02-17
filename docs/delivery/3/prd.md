# PBI-3: Tip-Over Boundaries and Safety Limits

## Overview
Implement tip-over boundaries and safety limits so the robot automatically stops or enters safe mode when tilt exceeds safe operating angles to prevent damage and ensure user safety.

## Problem Statement
Without safety limits, the robot could attempt to balance at extreme angles, causing damage to motors, frame, or components, and creating safety hazards.

## User Stories
- As a developer, I want automatic safety limits, so that the robot stops when tilt exceeds safe angles.
- As a user, I want the robot to recover safely, so that it can return to normal operation after being disturbed.

## Technical Approach
- Define maximum safe tilt angles (pitch/roll) based on mechanical limits (e.g., ±30°).
- Implement tilt boundary detection in balance controller.
- Add automatic stop/safe mode when boundaries exceeded (disable motors, enter standby).
- Implement recovery routine to return to safe operating range.
- Add visual/audio feedback when approaching boundaries (optional: LED/buzzer).

## UX/UI Considerations
- Safety feedback: LED indicators or buzzer when approaching limits.
- Recovery mode: clear indication when robot is attempting to recover.

## Acceptance Criteria
- Robot stops automatically when tilt exceeds defined boundaries.
- Safe mode activates correctly (motors disabled, balance controller paused).
- Recovery routine successfully returns robot to safe operating range.
- Boundary detection tested with physical tilting.
- All CoS from backlog met.

## Dependencies
- PBI 1 (Teensy Core Firmware) - requires working balance controller.
- PBI 2 (SPI Migration) - recommended for reliable IMU readings.

## Physical Robot Changes
- **No wiring changes required** - software-only implementation.
- **Physical testing required**: Must physically tilt robot to test boundary detection and recovery routines.
- **Safety considerations**: Testing should be done with robot secured or in safe environment to prevent damage.

## Open Questions
- Exact tilt angle limits (mechanical analysis required).
- Recovery strategy: automatic vs manual intervention.

## Related Tasks
See [Tasks for PBI 3](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


