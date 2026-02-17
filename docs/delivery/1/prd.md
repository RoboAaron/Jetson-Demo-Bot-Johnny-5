# PBI-1: Teensy Core Firmware Implementation

## Overview
Implement the core firmware on Teensy 4.1 for self-balancing control, USB serial interfacing with Jetson, and PS3 remote control integration.

## Problem Statement
The robot requires low-level real-time control for balancing, communication with the Jetson for higher-level autonomy, and manual override via PS3 controller to enable development and testing.

## User Stories
- As a developer, I want stable balancing using IMU and PID to control ESCs.
- As a developer, I want bidirectional comms with Jetson over USB serial for data exchange.
- As a developer, I want PS3 wireless control mapped to motor commands per teensy_gamepad_to_vesc_guide.md.

## Technical Approach
- Use Arduino/Teensyduino for firmware development.
- IMU: Adafruit BNO085 library for orientation data (I2C at 400kHz, 400Hz updates - optimized configuration).
- Balancing: PID loop computing currents sent to FSESC via UART (VescUart library).
- Jetson Interface: USB serial (high baud rate) with simple packet protocol for IMU upstream and commands downstream.
- PS3 Control (Temporary Solution): Jetson handles PS3 controller input (via USB or Bluetooth), sends velocity/steering commands to Teensy via USB serial. This avoids USBHost_t36 library conflict on Teensy.
- Future PS3 Control: When ESP32 WROOM module arrives, can implement direct Bluetooth PS3 control on Teensy if desired.
- Blending: Remote inputs as velocity setpoints for PID; Jetson commands for autonomy.

## Implementation Status & Accomplishments

### ✅ Completed (2025-01)
1. **IMU Integration**: BNO085 via I2C at 400kHz, achieving 400Hz update rate with 97.7-98.8% communication success
2. **Single-Loop PID Balance Controller**: Implemented and tuned to excellent performance
   - **Tuning Values**: Kp=1.50, Ki=0.00, Kd=0.03, baseSetpoint=-0.70°
   - **Performance**: ±0.5° roll stability, smooth motor control (-1.0A to +1.0A)
   - **Status**: Ready for velocity control (see `teensy_balance_single_loop/LAST_WORKING_CONFIG.md`)
3. **Python Tuning GUI**: Created comprehensive GUI (`tuning_code/robot_tuning_gui.py`) for:
   - Real-time parameter monitoring and adjustment
   - Live PID gain tuning via serial commands
   - Data logging and visualization
   - Support for cascaded velocity control
4. **Cascaded Velocity Control (Phase 1)**: Implemented outer velocity loop with angle loop
   - Velocity PID at 20Hz with EMA filtering, deadband, output clamping, slew rate limiting
   - Currently fixing sign issues and deadband mode thrashing (Task 4-2)

### 🔄 In Progress
- Velocity loop sign correction and deadband stability fixes
- Comprehensive testing and validation

### ⏳ Pending
- USB serial interface with Jetson (Task 1-3)
- PS3 remote control integration (Task 1-4)
- Mode blending and integration (Task 1-5)

## UX/UI Considerations
N/A (firmware-focused; debugging via serial monitor).

## Acceptance Criteria
- ✅ **Balancing maintains upright position**: Achieved ±0.5° roll stability (exceeds ±5° requirement)
- ⏳ USB serial exchanges data at >100 Hz without loss (pending Task 1-3).
- ⏳ PS3 controls robot movement smoothly, with failsafe stopping on disconnect (pending Task 1-4).
- ⏳ All CoS from backlog met (in progress - core balancing complete, integration pending).

## Physical Robot Changes
- **Initial wiring setup** (if not already complete):
  - BNO085 IMU wiring: Currently I2C (VCC, GND, SDA, SCL, INT, RST) - will migrate to SPI per PBI 2.
  - FSESC UART connections: Serial1/Serial2 to dual FSESC controllers.
  - USB connection: Teensy USB to Jetson (for serial communication and power).
  - PS3 controller: Connected to Jetson (USB or Bluetooth) - NOT to Teensy (avoids library conflict).
- **Physical testing required**: Balance testing, USB serial communication testing, PS3 controller testing via Jetson.

## Dependencies
- Teensy setup per setup_teensy_dev.md.
- FSESC configured for UART per guide.
- Libraries: VescUart, Adafruit_BNO08x (NOT USBHost_t36 - PS3 handled by Jetson).
- Jetson: ROS 2 or Python script to handle PS3 controller input and send commands to Teensy.

## Open Questions
- Exact PID tuning parameters (to be iterated in testing).
- Serial protocol details (e.g., JSON vs. binary).

## Related Tasks
See [Tasks for PBI 1](../delivery/1/tasks.md)

**Parent Backlog**: [Backlog.md](../delivery/backlog.md)

