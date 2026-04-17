# Tasks for PBI 4: Rock-Solid Balancing (99%+ Reliability)

This document lists all tasks associated with PBI 4.

**Parent PBI**: [PBI 4: Rock-Solid Balancing](./prd.md)

## Task Summary

| Task ID | Name | Status | Description |
|---------|------|--------|-------------|
| 4-1 | [Implement Cascaded Velocity Control (Phase 1)](./4-1.md) | Review | Add velocity control loop to working single-loop balance controller, enabling controlled forward/backward movement. |
| 4-2 | [Fix Velocity Loop Sign and Deadband Issues](./4-2.md) | Review | Fix velocity PID sign (REVERSE mode), deadband mode thrashing, input clamping, and BNO085 reboot recovery. |
| 4-3 | [Optimize PID Tuning for Stability](./4-3.md) | Proposed | Systematically tune angle and velocity PID gains to achieve smooth, stable balance. |
| 4-4 | [Implement Sensor Fusion](./4-4.md) | Proposed | Add Kalman or complementary filter for optimal angle estimation from IMU data. |
| 4-5 | [Add Velocity Damping (Kd_vel)](./4-5.md) | Proposed | Add derivative term to velocity loop to eliminate chattering and improve stability. |
| 4-6 | [Comprehensive Testing and Validation](./4-6.md) | Proposed | Test across all scenarios (smooth floor, uneven surfaces, inclines, disturbances) to achieve 99%+ reliability. |
| 4-7 | [Balance Recovery — EEPROM Reset, Dither, Gain Logging](./4-7.md) | Done | Force source defaults via SETTINGS_MAGIC bump, replace piecewise stiction comp with zero-mean 40 Hz dither, and add per-second effective-gains line to the serial stream. |
| 4-8 | [PID State Reset on Safety Transitions + PID Logic Validation](./4-8.md) | InProgress | Fix `PID_v1.Initialize()` DC-bias latching on every `MANUAL→AUTOMATIC` transition across all three PIDs, audit sign/clamp/filter paths, and expose `angleInput` in the stream for filter-vs-raw diagnosis. |
| 4-E2E | [E2E CoS Test](./4-E2E.md) | Proposed | Holistic verification of all PBI 4 CoS (99%+ reliability across all test scenarios). |

History:
- 2026-01-26: Task index created by AI_Agent based on current implementation status.
- 2026-04-16: 4-2 moved InProgress → Review (velocity sign/deadband/input-clamping + BNO085 reboot recovery complete). 4-7 added as InProgress for inner-loop EEPROM-gain-shadow + stiction-comp limit-cycle + log self-documentation fixes per expert evaluation of `tuning_code/logs/robot_log_20260415_222218.txt`.
- 2026-04-16: 4-7 moved InProgress → Done (all six requirements verified against field log `robot_log_20260416_223241.txt`). 4-8 added as InProgress for the `PID_v1.Initialize()` DC-bias latching bug discovered during 4-7 verification — stand-test screenshot showed `RollOut` pinned at `+maxCurrent` for > 10 minutes at near-zero error, traced to `outputSum = *myOutput` snapshot at every `MANUAL→AUTOMATIC` transition with `Ki=0, P_ON_E` making it a permanent DC bias.
