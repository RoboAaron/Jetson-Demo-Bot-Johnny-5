# Project Tasks
_Last updated: 2026-02-11_

> **Important**: The authoritative PRD and task management system lives in
> `docs/delivery/` on the **`feature/spi-migration`** branch — not on `master`.
> This file is a summary view. For full task details, consult those documents.
>
> This file was initially written from the wrong branch (`master`) and contained
> inaccurate information. It has been corrected to reflect the real project state.

---

## PRD System Location

| File | Description |
|------|-------------|
| `docs/delivery/backlog.md` | Single source of truth — 16 PBIs with status |
| `docs/delivery/1/prd.md` | PBI-1: Core Teensy firmware |
| `docs/delivery/1/tasks.md` | Tasks 1-1 through 1-5 |
| `docs/delivery/4/prd.md` | PBI-4: Rock-solid balancing (99%+ reliability) |
| `docs/delivery/4/tasks.md` | Tasks 4-1 through 4-6 |
| `docs/delivery/2–16/prd.md` | PRDs for all other PBIs |

Active development branch: **`feature/spi-migration`**

See `firmware/FIRMWARE_DESIGN.md` for firmware architecture decisions and stability assessment.

---

## PBI Backlog Status

| ID | Status | Summary |
|----|--------|---------|
| 1 | ✅ Agreed / Mostly Done | Core Teensy firmware: IMU + PID balance, USB serial, PS3 via Jetson |
| 2 | ❌ Rejected | SPI migration — abandoned; I2C at 400Hz is sufficient |
| 3 | Proposed | Tip-over safety limits (±30° auto-stop) |
| **4** | **🔄 InProgress** | **Rock-solid balancing (99%+ reliability)** |
| 5 | Proposed | Prioritize 8 demo use cases |
| 6 | Proposed | Aluminum chassis build and CoG rebalancing |
| 7 | Proposed | Isaac Sim URDF/SDF model and simulation |
| 8–15 | Proposed | 8 core demos (Vision, Human Following, Conversational AI, Object Recognition, Teleoperation, Sensor Fusion, Voice Command, Safety+Recovery) |
| 16 | Proposed | SLAM with LDROBOT STL-19P/D500 + SLAM Toolbox |

---

## PBI-1 Task Status (Core Firmware)

| Task | Status | Description |
|------|--------|-------------|
| 1-1 | ✅ Done | Teensy project skeleton |
| 1-2 | ✅ Done | IMU reading + PID balancing (400Hz I2C, Kp=1.50, Ki=0.00, Kd=0.03) |
| 1-3 | Proposed | USB serial interface with Jetson |
| 1-4 | Proposed | PS3 remote control integration |
| 1-5 | Proposed | Mode blending and failsafes |
| 1-E2E | Proposed | End-to-end CoS test |

---

## PBI-4 Task Status (Rock-Solid Balancing) — Active Focus

| Task | Status | Description |
|------|--------|-------------|
| 4-1 | ✅ Review | Cascaded velocity control Phase 1 + motor direction fix |
| **4-2** | **🔄 InProgress** | Fix velocity PID sign (REVERSE mode), deadband thrashing, input clamping |
| 4-3 | Proposed | Systematic PID tuning for stability |
| 4-4 | Proposed | Sensor fusion (Kalman/complementary filter) |
| 4-5 | Proposed | Add Kd_vel > 0 to eliminate 14Hz chattering |
| 4-6 | Proposed | Comprehensive scenario testing (99%+ reliability validation) |
| 4-E2E | Proposed | End-to-end CoS test |

**Current baseline**: 30–40% success rate, 30+ second runs achieved, chattering at ~14Hz.
**Active firmware**: `teensy_balance_cascaded/teensy_balance_cascaded.ino` on `feature/spi-migration`.

---

## Immediately Workable Tasks

Tasks that can be started now, in priority order:

1. **[4-2]** Fix velocity PID sign + deadband in `teensy_balance_cascaded.ino`
2. **[4-3]** Systematic PID re-tuning after 4-2 is verified on hardware
3. **[4-5]** Add Kd_vel > 0 to eliminate motor chattering
4. **[1-3]** USB serial interface with Jetson (needed for all demos)
5. **[PBI-5]** Prioritize and sequence the 8 demo use cases before starting PBIs 8–15
