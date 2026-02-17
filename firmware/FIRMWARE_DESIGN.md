# Firmware Design — Findings and Decisions
_Last updated: 2026-02-11_

> **Note**: This document was initially written based on the wrong branch (`master`),
> which contains only documentation with no firmware code. All analysis below reflects
> the **real** firmware on `feature/spi-migration`, which is the active development branch.
> The previous version of this document contained speculative design decisions that did
> not match reality and has been replaced with this accurate record.

---

## 1. Where the Real Work Lives

| Branch | Purpose |
|--------|---------|
| `feature/spi-migration` | **Active development branch** — all real firmware, PRDs, tasks, tuning GUI, logs |
| `pbi1-task1-2` | Earlier task branch (now superseded by feature/spi-migration) |
| `master` / `main` | Documentation only — no firmware code |

### Real PRD/Task Management System
The project uses a structured delivery system in `docs/delivery/` on `feature/spi-migration`:

```
docs/delivery/
├── backlog.md          ← Single source of truth: 16 PBIs with status
├── 1/
│   ├── prd.md          ← PBI-1: Core Teensy firmware
│   └── tasks.md        ← Tasks 1-1 through 1-5+E2E
├── 4/
│   ├── prd.md          ← PBI-4: Rock-solid balancing (99%+ reliability)
│   └── tasks.md        ← Tasks 4-1 through 4-6+E2E
└── 2-16/               ← PRDs for all other PBIs (2=Rejected, 3-16=Proposed)
```

---

## 2. Actual Firmware — What Exists

### Firmware Files on `feature/spi-migration`

| Directory | Status | Description |
|-----------|--------|-------------|
| `teensy_balance_cascaded/` | **Active** | Current primary firmware — Task 4-2 in progress |
| `teensy_balance_logging/` | Archived/SPI | Abandoned SPI migration experiment (PBI-2 Rejected) |
| `teensy_balance_logging_i2c_optimized/` | Reference | Intermediate I2C-optimized version before cascaded |
| `teensy_balance_single_loop/` | Reference | Original single-loop PID (working baseline) |
| `archive/teensy_balance_archive/` | Archived | Pre-optimisation baseline |
| `motor_characterization_test/` | Utility | Motor direction/characterisation test sketch |
| `teensy_spi_*/`, `teensy_i2c_*/` | Diagnostic | Diagnostic sketches from debugging |

### Active Firmware: `teensy_balance_cascaded/teensy_balance_cascaded.ino`
~1320 lines. This is the **only file that should be modified** for ongoing PBI-4 work.

---

## 3. Actual Architecture (What Is Really Implemented)

### Libraries Used (Reality vs. Earlier Speculation)

| Component | What the Code Actually Uses | What Was Previously Speculated (Wrong) |
|-----------|----------------------------|-----------------------------------------|
| IMU driver | `Adafruit_BNO08x` | SparkFun BNO08x Arduino Library |
| VESC comms | `VescUart` (UART) | FlexCAN_T4 (CAN) |
| PID | `PID_v1.h` (Arduino PID library) | Custom PID implementation |
| EEPROM | `EEPROM.h` (built-in) | N/A |

**Why `Adafruit_BNO08x` not SparkFun**: The SparkFun BNO08x library had
conflicts with Teensy's USB stack, documented in commit `8838854`. The
Adafruit version works correctly on Teensy 4.1.

**Why `VescUart` not CAN**: The FSESC is connected via UART serial
(not CAN bus) on the current hardware setup. CAN bus wiring is planned but
UART was used for initial development. See `VESC_COMMUNICATION_RESEARCH.md`
on `feature/spi-migration`.

### Control Architecture

```
BNO085 (I2C 400kHz, 400Hz)
    │
    ▼ roll angle (degrees)
    │
    ├─────────────────────────────────────────────────────────┐
    │                                                         │
    │ VELOCITY LOOP (20Hz / 50ms)                             │ ANGLE LOOP (500Hz / 2ms)
    │                                                         │
    │  VESC encoder ERPM → mech RPM → m/s                    │  activeSetpoint = baseSetpoint + angleSetpointFromVel
    │  EMA filter (α=0.1)                                    │  Kp=1.50, Ki=0.00, Kd=0.03
    │  Deadband (±0.08 m/s, state-machine)                   │  → motorCurrent (Amps)
    │  Kp_vel=1.0, Ki_vel=0.0, Kd_vel=0.0 (REVERSE mode)   │
    │  Output clamp ±0.5°, slew 0.05°/update                 │
    │  → angleSetpointFromVel                                 │
    └────────────────────────────────────────────────────────┘
                                │
                                ▼ motorCurrent (Amps)
               ┌────────────────────────────────────────┐
               │ Stiction compensation: ≥0.55A to move  │
               └────────────────────────────────────────┘
                                │
               ┌────────────────┴────────────────┐
               ▼                                  ▼
      vescLeft.setCurrent(-c)          vescRight.setCurrent(c)
      (LEFT_MOTOR_DIRECTION_SIGN=1.0)  (RIGHT_MOTOR_DIRECTION_SIGN=-1.0)
```

**Optional Yaw PID**: Disabled by default (Kp_yaw=Ki_yaw=Kd_yaw=0.0). Differential
current splitting available if enabled.

### Key Parameter Values (Current Working Configuration)

From `LAST_WORKING_CONFIG.md` and active `teensy_balance_cascaded.ino`:

| Parameter | Value | Notes |
|-----------|-------|-------|
| baseSetpoint | -0.70° | Balance point trim (negative = slight forward lean) |
| Kp (angle) | 1.50 | Inner loop proportional |
| Ki (angle) | 0.00 | Inner loop integral (disabled) |
| Kd (angle) | 0.03 | Inner loop derivative |
| Kp_vel | 1.0 | Outer loop proportional |
| Ki_vel | 0.0 | Outer loop integral (disabled) |
| Kd_vel | 0.0 | Outer loop derivative (disabled — chattering issue) |
| maxCurrent | 6.5A | Output clamp (FSESC rated 50A) |
| MIN_DRIVE_CURRENT | 0.55A | Stiction breakaway compensation |
| angleSetpoint output clamp | ±0.5° | Velocity loop output limit |
| IMU rate | 400Hz | I2C at 400kHz |
| Angle PID rate | 500Hz (2ms) | Inner loop |
| Velocity PID rate | 20Hz (50ms) | Outer loop |
| VESC read interval | 15ms (67Hz) | encoder feedback |
| WHEEL_DIAMETER | 0.165m | From VESC XML config |
| MOTOR_POLES | 30 | From VESC XML config |

---

## 4. Decisions Made During Development (Chronological)

### I2C vs. SPI for BNO085 (PBI-2, Rejected)
**Decision**: Stay on I2C. SPI migration abandoned.
**Reason**: SPI wiring is hardware-version specific on the Teyleten GY-BNO085 board
(PS0/PS1 jumpers require modification). I2C at 400kHz/400Hz proved sufficient:
97.7–98.8% communication success rate. SPI diagnostic sketches exist in repo for
reference.

### UART vs. CAN for VESC Communication
**Decision**: UART (`VescUart` library).
**Reason**: UART was available and working for initial development. CAN was planned
but not implemented yet. The `VescUart` library provides `getVescValues()` which
returns encoder ERPM, current, and voltage — sufficient for velocity feedback.

### Single-Loop vs. Cascaded PID
**Decision**: Cascaded (velocity outer → angle inner).
**Reason**: Single-loop achieved ±0.5° stability but drifted (no velocity control).
Literature review (`0e2da80`) identified missing velocity loop as root cause.
Cascaded architecture (velocity error → angle setpoint offset) is the industry standard
for self-balancing robots.

### Control Mode: Angle Setpoint from Velocity PID
**Decision**: Velocity PID outputs an *angle setpoint offset*, not a direct current.
**Reason**: The velocity loop adjusts how much the robot leans (tilt = thrust), which
the angle loop then executes. This is the standard "lean-to-move" architecture.

### Motor Direction
**Decision**: `RIGHT_MOTOR_DIRECTION_SIGN = -1.0`, `LEFT_MOTOR_DIRECTION_SIGN = 1.0`
**Reason**: Physical wiring means the right motor runs reverse relative to left for
forward motion. Fixed in Task 4-1 (commit `f672d65`, 2026-01-28).

### IMU Axis: Roll = Balance Axis
**Decision**: Roll angle used for balance (not pitch).
**Reason**: Physical mounting orientation — the robot's forward/back lean is the
BNO085 roll axis as mounted. `baseSetpoint = -0.70°` accounts for center of mass offset.

---

## 5. Current Stability Assessment

**Baseline (as of Task 4-1 completion, 2026-01-28)**:
- Balance success rate: **30–40%** (functional, chattering at ~14Hz)
- Maximum continuous balance: **30+ seconds achieved**
- Motor direction: **Fixed** ✅
- Cascaded control: **Implemented** ✅
- Velocity loop sign and deadband: **In progress (Task 4-2)** 🔄

**Verdict: Not yet stable enough for demo use cases (PBIs 8–15), but actively
approaching it. The core balance loop works; the velocity control layer is the
current blocker.**

### Known Issues (as of 2026-02-11)

| Issue | Root Cause | Status | Task |
|-------|-----------|--------|------|
| Motor chattering at 14Hz | Kd_vel=0 (no velocity damping) | Pending | 4-5 |
| Velocity loop sign | REVERSE mode verification | In Progress | 4-2 |
| Deadband thrashing | State machine fix needed | In Progress | 4-2 |
| 30-40% success rate | Tuning not yet optimized | Pending | 4-3 |
| No sensor fusion | Raw IMU angle only | Pending | 4-4 |

### Path to 99% Reliability (PBI-4 Remaining Tasks)

| Task | Priority | Description |
|------|----------|-------------|
| **4-2** | **Now** | Fix velocity PID sign, deadband thrashing, input clamping |
| **4-3** | After 4-2 | Systematic PID re-tuning (angle + velocity gains) |
| **4-5** | After 4-3 | Add Kd_vel > 0 to eliminate 14Hz chattering |
| 4-4 | Parallel | Sensor fusion (Kalman/complementary filter) |
| 4-6 | After 4-5 | Comprehensive scenario testing |

---

## 6. Tuning Interface

The firmware exposes a serial CLI (115200 baud) and a Python GUI:
- `tuning_code/robot_tuning_gui.py` — real-time plot + parameter adjustment
- `tuning_code/run_gui.sh` — launcher

Key serial commands in `teensy_balance_cascaded`:
- `p/P` — Kp angle ±step, `i/I` — Ki angle, `j/D` — Kd angle
- `w/W` — Kp_vel, `e/E` — Ki_vel
- `z/Z` — baseSetpoint, `m/M` — maxCurrent
- `x` — show all settings, `@` — machine-readable sync for GUI
- `k/g` — EEPROM save/load
- `d` — toggle diagnostic mode (extra VEL/VEL_COMP debug prints)
- `l/SPACE/s/w/c` — logging commands

---

## 7. What This Document Replaces

The previous version of `FIRMWARE_DESIGN.md` was created before the real firmware
branches were found. It contained entirely speculative architecture decisions based on
documentation only. Specific errors in the previous version:
- Claimed no firmware existed (wrong — extensive firmware on `feature/spi-migration`)
- Specified SparkFun BNO08x library (wrong — Adafruit_BNO08x is used)
- Specified FlexCAN_T4 / CAN bus (wrong — VescUart / UART is used)
- Specified custom PID (wrong — PID_v1.h Arduino library is used)
- Created a firmware skeleton (removed — it duplicated real work incorrectly)
- Flagged BNO055 in setup_teensy_dev.md as a bug (partially correct — BNO055
  is wrong, but the right library is Adafruit_BNO08x not SparkFun BNO08x)
