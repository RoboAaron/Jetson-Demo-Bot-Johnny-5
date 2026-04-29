# Jetson Demo Bot - Johnny 5

Self-balancing robot platform using a Teensy 4.1 for real-time control and Jetson for higher-level features.

## System Overview

### Hardware

- Jetson (Nano/Orin): higher-level compute, ROS2 stack, integration tooling
- Teensy 4.1: hard real-time balance and motor control loops
- BNO085 IMU: fused orientation + gyro data
- Dual FSESC motor controllers: current control over UART
- LDROBOT LiDAR: mapping/navigation integration path

### Communication

- Jetson <-> Teensy: USB serial at 2,000,000 baud
- Teensy <-> ESCs: UART at 115200 baud
- Teensy <-> IMU: I2C

## Current Control Status

- The active balance firmware is `teensy_balance_cascaded/teensy_balance_cascaded.ino`.
- Balance loop behavior is now stable after:
  - BNO085 reset recovery fixes (`wasReset()` report re-enable sequence)
  - PID state-reset fix for `PID_v1` MANUAL->AUTOMATIC transitions
  - dither-based low-speed drive assist replacing step-style stiction compensation
  - field-tuned defaults + settings version bump (`SETTINGS_MAGIC`)
- Current focus is to build velocity/position features on top of the stable balance loop.

## Quick Start

### Flash Teensy firmware (Linux)

```bash
cd teensy_balance_cascaded
arduino-cli compile --fqbn teensy:avr:teensy41 .
teensy_loader_cli --mcu=TEENSY41 -w -v teensy_balance_cascaded.ino.hex
```

### Run tuning GUI

```bash
python3 tuning_code/robot_tuning_gui.py
```

Important: only one host can own the Teensy USB connection at a time (Jetson or development laptop).

## Repository Layout

```text
teensy_balance_cascaded/        Active balance firmware
tuning_code/                    GUI + comms + log evaluation tools
ldrobot_lidar_ros2/             LiDAR setup and ROS2 launch/config scripts
firmware/                       Firmware design docs and patch artifacts
docs/delivery/                  Backlog, PBI/task tracking, implementation records
docs/hardware/                  Wiring and hardware integration notes
docs/setup/                     Setup/config guides
```

## Setup Entry Points

- Jetson/Linux setup guide: `setup_ubuntu_dev.md`
- Windows host setup script: `setup_windows_dev.bat`
- FSESC setup notes: `docs/setup/setup_fsesc_config.md`
- Hardware direction map: `docs/hardware/motor_direction_configuration.md`

## Planning and Design Docs

- Backlog (source of truth): `docs/delivery/backlog.md`
- PBI-4 balance work: `docs/delivery/4/prd.md`
- Task index for PBI-4: `docs/delivery/4/tasks.md`
- Balance roadmap: `docs/delivery/4/BALANCE_TUNING_ROADMAP.md`

## Branching Note

`main` is the consolidated source of truth for current design and implementation. The `v0.1-balance-working` tag marks the milestone where the balance loop reached stable field behavior.
