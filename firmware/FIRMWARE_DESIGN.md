# Firmware Design Decisions
_Last updated: 2026-02-11_

## 1. State of the Repository

**Finding**: As of 2026-02-11, **no firmware source code exists in this repository**.
The repo contains only documentation and the `ldrobot_lidar_ros2` ROS 2 package.

The user described existing work ("taking in IMU data, taking in VESC data, and driving
commands to the VESC") that is not yet committed here. This skeleton is designed to be
compatible with that description and the planning documents, and should be reconciled
with any local firmware once committed.

---

## 2. IMU Library Discrepancy — Decision Required

**Problem identified**: The planning docs specify two different sensors in different places.

| Document | What it says |
|----------|-------------|
| `README.md` | "BNO085 9DOF AHRS sensor" |
| `robotics_project_overview.md` | "BNO085 9DOF AHRS" |
| `robotics_inventory_costs.md` | References BNO085 |
| `setup_teensy_dev.md` (example code) | Uses `Adafruit_BNO055` library |

**These are different chips and incompatible libraries:**

| | BNO055 | BNO085 (= BNO080) |
|--|--------|-------------------|
| Manufacturer | Bosch | Hillcrest Labs / Bosch |
| Library | Adafruit BNO055 | SparkFun BNO08x |
| Interface | I2C/SPI, register-based | I2C/SPI/UART, report-based |
| Output | Euler + raw sensors | Fused quaternion, ARVR-stabilized rotation vector |
| Accuracy | ~2.5° | ~1° (ARVR stabilized) |
| Calibration | Manual, stored in registers | Continuous background calibration |
| Protocol | Direct register reads | Shtp/HID-style report requests |

**Decision**: **Use BNO085** (matches the hardware BOM; superior sensor).
The `Adafruit_BNO055` example in `setup_teensy_dev.md` was a placeholder/error.

**Corrective action**: Updated `setup_teensy_dev.md` to reference SparkFun BNO08x library.

**Library for PlatformIO**: `sparkfun/SparkFun BNO08x Arduino Library`

---

## 3. VESC CAN Protocol Decisions

The FSESC6.7 Pro is VESC-based and uses the open VESC CAN protocol.

### CAN Hardware
- **Interface**: Teensy 4.1 CAN1 (FlexCAN_T4 library)
- **Baud rate**: 500 kbps (per `setup_fsesc_config.md`)
- **CAN IDs**: Left motor = 1, Right motor = 2 (per `setup_fsesc_config.md`)
- **Library**: `tonton81/FlexCAN_T4` (native Teensy 4.x CAN FD controller)

### Control Mode: Current Control
For a self-balancing robot, **current control** is used (not RPM, not duty cycle).

Reasoning:
- Current ∝ torque → allows the balance loop to control acceleration directly
- RPM control adds an inner velocity loop with its own phase lag, destabilising balance
- Duty cycle control has non-linear response at low speeds
- Starting gains: ±20 A limit (conservative; FSESC rated 50 A per channel)

### VESC CAN Message Protocol
VESC standard protocol: CAN frame ID = `(COMM_PACKET_ID << 8) | VESC_CAN_ID`

| Packet | ID | Direction | Usage |
|--------|-----|-----------|-------|
| `CAN_PACKET_SET_CURRENT` | 0x01 | Teensy → VESC | Main balance command |
| `CAN_PACKET_SET_CURRENT_BRAKE` | 0x02 | Teensy → VESC | Active braking / fall hold |
| `CAN_PACKET_STATUS` | 0x09 | VESC → Teensy | RPM, current, duty (feedback) |
| `CAN_PACKET_STATUS_4` | 0x10 | VESC → Teensy | FET temp, motor temp, input current |

Payload encoding for `SET_CURRENT`: `int32_t` big-endian, scaled × 1000 (mA units).
Payload decoding for `STATUS`: RPM = `int32_t` × 1, current = `int16_t` / 10, duty = `int16_t` / 1000.

### Watchdog / Heartbeat
The VESC has a built-in watchdog: if no CAN command is received within its timeout window,
it coasts. The Teensy must send commands at ≥ 20 Hz to keep motors alive.
Configured timeout in `setup_fsesc_config.md`: 1000 ms. We send at 200 Hz — well within margin.

---

## 4. Balance Controller Architecture

### Control Loop
```
BNO085 (200 Hz)
    │
    ▼ pitch angle (deg)
PID controller  ← setpoint = 0° + trim
    │
    ▼ current command (A)
VESC CAN ──→ Left  motor (CAN ID 1)
         └──→ Right motor (CAN ID 2)
             (same command — both wheels driven identically for forward balance)
```

### Loop Frequency
- Target: **200 Hz** (IMU report rate drives the loop)
- Teensy 4.1 at 600 MHz has ample headroom for 200 Hz with math and CAN overhead

### PID Starting Gains
Values from `setup_fsesc_config.md` position PID section, adapted for current control:

| Gain | Starting Value | Notes |
|------|---------------|-------|
| Kp | 5.0 | Starting point; tune upward until oscillation, then back off |
| Ki | 0.5 | Low initially to prevent windup; increase for steady-state trim |
| Kd | 0.1 | Low; increase for damping if robot rocks |
| Output min | -20.0 A | Conservative limit vs 50 A ESC rating |
| Output max | +20.0 A | |
| Fall threshold | 45° | Beyond this, motors coast immediately |

These are **placeholder values** — hardware tuning is required.

### Integral Windup Protection
Integral is clamped to ±(MAX_CURRENT_A / Ki) to prevent windup during falls or
extended periods off-balance (e.g., robot being held up by hand).

---

## 5. Heading / Yaw (Phase 2 only)
The balance skeleton drives both motors identically. Yaw (turning) is deferred to
Phase 3 when the Jetson sends velocity commands via CAN or serial.

Architecture when Jetson integration is added:
- Jetson publishes `/cmd_vel` (linear.x, angular.z) via ROS 2
- Jetson↔Teensy bridge translates to: left_current = balance_output + yaw_component,
  right_current = balance_output − yaw_component

---

## 6. Serial Debug Interface (Minimal CLI)
The Teensy exposes a simple character command interface over USB serial (115200 baud):

| Key | Action |
|-----|--------|
| `e` | Toggle balance enable/disable |
| `p` | Print current PID gains |
| `+` / `-` | Increase / decrease Kp by 0.1 |
| `r` | Reset PID integrator |

This enables tuning without reflashing.

---

## 7. Readiness Assessment

**Verdict: Ready to create the firmware skeleton.**

Justification:
- Hardware is fully specified (BNO085, Teensy 4.1, FSESC6.7 Pro dual CAN)
- Communication protocol is documented and standard (VESC open protocol)
- Phase 2 scope is clear: balance on Teensy only, no Jetson dependency
- Existing documentation is sufficient to write meaningful stubs

**What is NOT ready (to be validated on hardware):**
- PID gains (placeholder values only — must be tuned physically)
- IMU mounting orientation / pitch sign convention (up vs down, forward vs back)
- Balance point trim offset (where the robot is actually "upright")
- CAN termination resistor placement verification
- Motor direction convention (which CAN ID is left/right; which sign is forward)

These are recorded in `TASKS.md` as hardware validation tasks.

---

## 8. Files Created

| File | Purpose |
|------|---------|
| `firmware/FIRMWARE_DESIGN.md` | This document — all design decisions |
| `firmware/teensy_balance/platformio.ini` | PlatformIO project config |
| `firmware/teensy_balance/src/main.cpp` | Main control loop |
| `firmware/teensy_balance/src/config.h` | All tunable constants and pin defs |
| `firmware/teensy_balance/src/imu_bno085.h` | BNO085 interface declaration |
| `firmware/teensy_balance/src/imu_bno085.cpp` | BNO085 driver (SparkFun BNO08x) |
| `firmware/teensy_balance/src/vesc_can.h` | VESC CAN interface declaration |
| `firmware/teensy_balance/src/vesc_can.cpp` | VESC CAN protocol implementation |
| `firmware/teensy_balance/src/balance_pid.h` | PID controller declaration |
| `firmware/teensy_balance/src/balance_pid.cpp` | PID controller implementation |
