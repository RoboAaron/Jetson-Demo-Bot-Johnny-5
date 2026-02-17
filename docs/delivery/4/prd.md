# PBI-4: Rock-Solid Balancing (99%+ Reliability)

## Overview
Achieve rock-solid balancing (99%+ reliability) through systematic improvements including sensor fusion, velocity damping, LQR controller, and extensive testing, so the robot maintains stability like commercial hoverboards/Segways.

## Problem Statement
**ORIGINAL**: Current balancing achieves ~30-40% success rate. Commercial hoverboards achieve 99%+ reliability through sensor fusion, high-rate control, and extensive tuning. We need to implement the complete pathway to 99% reliability.

**UPDATED (2026-01-26)**: Single-loop balance controller achieved excellent performance (±0.5° roll stability, 97.7-98.8% IMU communication success). Phase 1 cascaded velocity control has been implemented, enabling controlled forward/backward movement. Current focus is on fixing velocity loop sign issues and optimizing cascaded control for smooth operation. I2C optimization (400kHz, 400Hz) proved sufficient - SPI migration was abandoned due to board hardware limitations (see PBI 2).

## User Stories
- As a developer, I want sensor fusion for optimal angle estimation, so that noise and drift are minimized.
- As a developer, I want velocity damping to eliminate chattering, so that control is smooth and stable.
- As a developer, I want comprehensive testing, so that I can validate 99%+ reliability across all scenarios.

## Technical Approach
- ✅ **I2C Optimization**: Achieved 400kHz bus speed, 400Hz IMU updates (proven sufficient, SPI abandoned)
- ✅ **Phase 1 Cascaded Velocity Control**: Implemented velocity loop (outer) with angle loop (inner)
- 🔄 **Velocity Loop Fixes**: Fixing PID sign (REVERSE mode), deadband mode thrashing, input clamping
- ⏳ Implement sensor fusion (Kalman filter or complementary filter) combining gyro + accel data.
- ⏳ Add velocity damping (Kd_vel > 0) to eliminate motor chattering (currently Kd_vel = 0, PI-only).
- ⏳ Optimize PID tuning per `PATH_TO_99_PERCENT_RELIABILITY.md` target values.
- ⏳ Implement LQR controller for advanced control (optional, after PID optimization).
- ⏳ Conduct comprehensive testing: smooth floor, uneven surfaces, inclines, disturbances, extended operation.
- ⏳ Document tuning parameters and test results.

## Current Implementation Status

### ✅ Completed
1. **I2C Optimization**: 400kHz bus, 400Hz IMU updates, 500Hz PID rate
2. **Single-Loop Balance**: Excellent performance (Kp=1.50, Ki=0.00, Kd=0.03, ±0.5° stability)
3. **Phase 1 Cascaded Velocity Control**: 
   - Velocity PID at 20Hz with EMA filter (α=0.1)
   - Deadband logic (0.08 m/s when setpoint=0)
   - Output clamping (±0.5°)
   - Slew rate limiting (0.05° per update)
   - VESC encoder reading and ERPM→m/s conversion
4. **Python Tuning GUI**: Real-time monitoring and parameter adjustment

### 🔄 In Progress (Task 4-2)
- Fixing velocity PID sign (DIRECT→REVERSE)
- Fixing deadband mode thrashing (state flag)
- Adding input clamping (-2.0 to +2.0 m/s)
- Sign verification debugging

### ⏳ Pending
- Sensor fusion implementation
- Velocity damping (Kd_vel > 0)
- Comprehensive testing and validation
- LQR controller (optional)

## UX/UI Considerations
- Tuning interface: serial commands for live PID adjustment.
- Test logging: data logging for analysis and validation.

## Acceptance Criteria
- ✅ **Phase 1 Cascaded Velocity Control**: Implemented and functional (needs sign/deadband fixes).
- ⏳ Sensor fusion implemented and tuned.
- ⏳ Velocity damping eliminates chattering (Kd_vel > 0).
- ⏳ PID tuning optimized to target values.
- ⏳ 99%+ success rate across all test scenarios.
- ⏳ LQR controller implemented (optional advanced feature).
- ⏳ All CoS from backlog met.

## Dependencies
- PBI 1 (Teensy Core Firmware) - ✅ **COMPLETE** - working balance controller achieved.
- PBI 2 (SPI Migration) - **ABANDONED** - I2C at 400Hz provides sufficient performance.
- `PATH_TO_99_PERCENT_RELIABILITY.md` - implementation guide.
- **Balance code review**: [BALANCE_CODE_REVIEW.md](../../../BALANCE_CODE_REVIEW.md) — Phase 0 immediate fixes (rate-limit writes, filter alpha, PID reset on cutoff). See [BALANCE_REVIEW_ACTIONS.md](./BALANCE_REVIEW_ACTIONS.md) for issue→task mapping. Phase 4 (SPI) is out of scope.

## Physical Robot Changes
- **No wiring changes required** - software-only implementation (assumes PBI 2 SPI migration complete).
- **Physical testing required**: Comprehensive testing across multiple scenarios:
  - Smooth floor (baseline)
  - Uneven surfaces
  - Inclines (up to 10°)
  - Disturbance rejection (pushes from various angles)
  - Extended operation (30+ minutes continuous)
  - Different loads (weight variations)
  - Surface friction variations (tile, carpet, etc.)
- **Mechanical verification**: May require verification of:
  - Center of mass height (<15cm ideal)
  - Symmetrical weight distribution
  - Motor alignment (parallel, no toe-in/toe-out)
  - Zero backlash in motor coupling
  - Wheel diameter matching (<1mm difference)
  - Frame rigidity (no flex)

## Open Questions
- Kalman filter vs complementary filter (complexity vs performance tradeoff).
- LQR implementation timeline (after PID or in parallel).

## Related Tasks
See [Tasks for PBI 4](./tasks.md).

**Parent Backlog**: [Backlog.md](../backlog.md)


