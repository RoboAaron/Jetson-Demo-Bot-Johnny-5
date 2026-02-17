# PBI-6: Aluminum Chassis Build and Rebalance

## Overview
Build the aluminum chassis and rebalance the robot's center of mass so the robot has a rigid, crash-resistant frame with optimal weight distribution for stable balancing.

## Problem Statement
The robot needs a proper chassis with optimal center of mass (CoG) positioning for stable balancing. Current prototype may not have optimal weight distribution or structural rigidity.

## User Stories
- As a developer, I want a rigid aluminum chassis, so that the robot can withstand crashes and maintain structural integrity.
- As a developer, I want optimal CoG positioning, so that balancing is stable and reliable.

## Technical Approach
- Fabricate/cut 3mm aluminum base plate per design specifications.
- Fabricate/cut top plate (6mm Lexan or 3mm aluminum - decision required).
- Assemble plate + standoff construction with proper deck stacking.
- Mount all components (batteries, FSESC, Jetson, Teensy, sensors) per design.
- Measure and verify center of mass height (<15cm ideal) and alignment over wheel axis.
- Verify symmetrical weight distribution.
- Test balance with new chassis and adjust component placement if needed.
- Document chassis assembly and CoG measurements.

## UX/UI Considerations
- Component placement: ensure accessibility for maintenance and debugging.
- Cable management: clean routing to avoid interference.

## Acceptance Criteria
- Chassis fabricated and assembled per design.
- All components mounted securely.
- CoG height <15cm and aligned over wheel axis.
- Symmetrical weight distribution verified.
- Balance tested and validated with new chassis.
- All CoS from backlog met.

## Dependencies
- Design specifications from `robotics_design_methodology.md`.
- All hardware components available (batteries, FSESC, Jetson, Teensy, sensors).

## Open Questions
- Top plate material: 6mm Lexan vs 3mm aluminum (weight vs optical clarity).
- Exact CoG target height (may need iteration).

## Related Tasks
See [Tasks for PBI 6](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


