# Tasks for PBI 1: Teensy Core Firmware Implementation

This document lists all tasks associated with PBI 1.

**Parent PBI**: [PBI 1: Teensy Core Firmware Implementation](./prd.md)

## Task Summary

| Task ID | Name | Status | Description |
|---------|------|--------|-------------|
| 1-1 | [Setup Teensy Project Skeleton](./1-1.md) | Done | Initialize Arduino project with required libraries and basic structure. |
| 1-2 | [Implement IMU Reading and PID Balancing](./1-2.md) | Done | ✅ **COMPLETED**: IMU integration (BNO085 via I2C at 400Hz) and PID control loop for self-balancing implemented. Single-loop balance controller working with Kp=1.50, Ki=0.00, Kd=0.03, achieving excellent stability (±0.5° roll). VESC motor control via UART implemented. Python tuning GUI created for live parameter adjustment. |
| 1-3 | [Implement USB Serial Interface with Jetson](./1-3.md) | Proposed | Add bidirectional USB serial communication for data exchange with Jetson. |
| 1-4 | [Integrate PS3 Remote Control](./1-4.md) | Proposed | Add wireless PS3 controller input per guide, mapping to motor commands. |
| 1-5 | [Mode Blending and Integration](./1-5.md) | Proposed | Implement blending of balancing, remote, and Jetson inputs; add failsafes. |
| 1-E2E | [E2E CoS Test](./1-E2E.md) | Proposed | Holistic verification of all PBI CoS. |

History:
- 2025-09-28: Task index created by AI_Agent.
