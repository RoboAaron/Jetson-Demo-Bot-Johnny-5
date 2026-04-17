# Balance tuning roadmap (PBI 4)

**Parent**: [PBI 4 tasks](./tasks.md)  
**Purpose**: Single reference for the phased path from “first inner loop works” to driveable robot, plus the CLI-first tuning procedure used in the field.  
**Source**: Session guidance after `robot_log_20260416_231027.txt` (pre–setpoint/Kd/Filt change) and validation in `robot_log_20260416_232745.txt` (post-change).

---

## CLI-first tuning procedure (inner angle loop)

Use the tuning GUI or serial commands so every run is captured in logs with `GAINS:` lines.

1. **Setpoint = CG** — Adjust angle setpoint until quiet-hold mean `RollOut` is near zero (within ±0.05 A over ~5 s). P-only with `Ki=0` cannot remove static lean without matching the mechanical balance angle.
2. **Filter alpha** — Increase `angleFilterAlpha` (e.g. 0.15 → 0.25) to reduce `AI` lag vs raw `R` during pushes; watch for motor chatter. If `|AI−R|` stays large during transients, consider 0.30, then back off if noisy.
3. **D gain** — Raise `Kd` in steps (e.g. 0.03 → 0.06) for damping on disturbances. If recovery still pegs `±maxCurrent`, next lever is gyro-rate D (Phase B), not only higher `Kd` on filtered angle.
4. **Kp** — After steps 1–3, increase `Kp` in small steps until mild oscillation at rest, then back off ~30%.
5. **`maxCurrent`** — Keep limited on low-traction surfaces (cardboard). Re-tune on the real floor before raising authority.

**Log diagnostics** (from stream fields):

- Quiet hold: mean `RollOut` vs zero; `Err` vs `RollOut` should match `Kp·Err` when rate is small.
- Transient: `|AI−R|` during fast rolls (filter lag).
- Recovery: time spent at `|RollOut| = maxCurrent`; peak `|R|` and whether oscillation decays.

---

## Phase A — Quiet balance + mild disturbance recovery

**Goal**: Hold within ±0.3° of setpoint for 30 s; recover from ±1.5° impulse without sustained saturation.

**Knobs**: `Kp`, `Kd`, `angleFilterAlpha`, `baseSetpoint`, dither amplitude/frequency, `maxCurrent`.

**Related tasks**: [4-3](./4-3.md) (systematic tuning), [4-8](./4-8.md) (PID state reset; complete verification when bench criteria met).

---

## Phase B — Gyro-rate derivative on the angle loop

**Goal**: Use BNO085 calibrated gyro rate for damping instead of (or in addition to) `PID_v1`’s derivative on lag-filtered angle.

**Why**: Filtered `dAI/dt` lags under aggressive pushes; gyro gives high-rate, low-lag tilt rate for the same job.

**Related task**: [4-9](./4-9.md).

---

## Phase C — Floor / traction transition and plant ID

**Goal**: Re-tune on non-cardboard surface; optional log of commanded current vs wheel acceleration for a simple force map.

**Related task**: [4-10](./4-10.md).

---

## Phase D — Velocity outer loop (drift / position box)

**Goal**: Keep the robot in a position box at rest; step velocity commands without excessive lean.

**Related tasks**: [4-1](./4-1.md), [4-2](./4-2.md), [4-5](./4-5.md).

---

## Phase E — Yaw and drive UX

**Goal**: Enable yaw PID when inner loop is solid; map GUI or RC to `velocitySetpoint` / `yawSetpoint`.

**Related tasks**: yaw portions of [4-3](./4-3.md), [4-6](./4-6.md).

---

## Phase F (stretch) — Full state feedback / observer

**Goal**: LQR or observer-based control once states `[θ, θ̇, x, ẋ]` are trustworthy.

**Related tasks**: [4-4](./4-4.md), broader [4-6](./4-6.md).

---

## Field log references

| Log | Notes |
|-----|--------|
| `tuning_code/logs/robot_log_20260416_231027.txt` | `Kp=1.5`, `Kd=0.03`, `Filt=0.15`, `Setpt=-0.70`; strong quiet hold but large filter lag on push; long ±`maxCurrent` oscillation. |
| `tuning_code/logs/robot_log_20260416_232745.txt` | `Kp=1.5`, `Kd=0.06`, `Filt=0.25`, `Setpt=-0.84`; improved authority use and faster settling after disturbance (see task index History). |

---

## Tooling / ops backlog (non-control)

| Item | Task |
|------|------|
| Tuning GUI lag (plot history, redraw cost) | [4-11](./4-11.md) |

---

## Revision history

- 2026-04-16: Initial roadmap from balance tuning session; proposed tasks 4-9–4-11 added to [tasks.md](./tasks.md).
- 2026-04-16: Firmware `SETTINGS_MAGIC = 0xC45C4DF0` forces source defaults aligned with `robot_log_20260416_232745.txt` (`Kp=1.5`, `Kd=0.06`, `Setpt=-0.84`, `Filt=0.25`, `maxCurrent=2`).
