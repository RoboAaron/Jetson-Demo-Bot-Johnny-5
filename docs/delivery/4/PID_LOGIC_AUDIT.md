# PID Logic Audit (4-8)

**Scope**: `teensy_balance_cascaded/teensy_balance_cascaded.ino`.
Purpose: one-time walk-through of every `PID_v1` instance in the
firmware, covering pointer hookup, sign convention, output clamping,
state reachability, and filter freshness. Written as the audit
artifact for Task 4-8 so future sessions don't re-derive it.

Library under inspection: `~/Arduino/libraries/PID/PID_v1.cpp`.
Relevant library facts:
- `PID::SetMode(AUTOMATIC)` calls `PID::Initialize()` once on every
  `MANUAL → AUTOMATIC` transition.
- `PID::Initialize()` sets `outputSum = *myOutput` and
  `lastInput = *myInput`. Also clamps `outputSum` to `outMin/outMax`.
- `PID::Compute()` with `P_ON_E` (default) does:
    ```
    outputSum += ki * error;
    outputSum = clamp(outputSum, outMin, outMax);
    output = kp * error + outputSum - kd * dInput;
    output = clamp(output, outMin, outMax);
    ```
  With `ki == 0`, `outputSum` is only written by `Initialize()` and
  by `SetOutputLimits()` clamping. It is otherwise a constant offset.

## 1. balancePID (inner loop, 500 Hz)

**Instance**:
```
PID balancePID(&pidAngleInput, &pidMotorCurrent, &pidAngleSetpoint,
               Kp, Ki, Kd, DIRECT);
```

**Pointer hookup**:
- `*myInput = &pidAngleInput` — updated by ISR from the EMA-filtered
  `angleInput` (which in turn is fused from `roll` with coefficient
  `angleFilterAlpha`, default 0.15). Also snapped by
  `rearmBalancePID()` at every arm so `Initialize()` sees the live
  value.
- `*myOutput = &pidMotorCurrent` — read by ISR, cast to `float`,
  written to `volatile float motorCurrent`. `rearmBalancePID()`
  zeroes this before every `SetMode(AUTOMATIC)`.
- `*mySetpoint = &pidAngleSetpoint` — updated from `angleSetpoint`
  (which is `baseSetpoint + angleSetpointFromVel`).

**Direction: DIRECT.** Justification: on our geometry, a positive
roll (leaning forward) requires positive motor torque (drive wheels
forward to move the base under the CG). `error = input - setpoint`
internally, so positive error (too-positive roll) produces positive
output → forward current → correct. Verified consistent with
observed behavior across 4-2 and 4-7 logs.

**Output clamp**: `SetOutputLimits(-maxCurrent, +maxCurrent)` called
in `setup()` and on every CLI `maxCurrent` change. Library clamps
`outputSum` and `output` separately.

**outputSum reachability**:
- `Ki = 0.0` by default (and locked there for 4-7/4-8 tuning).
- `P_ON_E` (default constructor): `outputSum` receives no contribution
  from `Compute()`.
- Therefore `outputSum` is controlled exclusively by
  `Initialize()` (via our helper) and `SetOutputLimits()` clamping.

**Rearm discipline**: every `SetMode(AUTOMATIC)` goes through
`rearmBalancePID()`. This zeroes `pidMotorCurrent`, snaps
`pidAngleInput = angleInput`, sets
`pidAngleSetpoint = angleSetpoint`, then calls `SetMode(AUTOMATIC)`.
Verified by grep: `balancePID.SetMode(AUTOMATIC)` appears only on
line 330 (inside the helper).

**Filter freshness**: `angleInput` is updated in the IMU event
handler synchronously when a rotation-vector report arrives. ISR
reads whatever the latest handler-produced value is. With 400 Hz IMU
stream and 500 Hz ISR, typical staleness is one ISR tick (2 ms). The
new `AI:` field in the stream exposes this for inspection.

## 2. velocityPID (outer loop, 50 Hz)

**Instance**:
```
PID velocityPID(&velocityInput, &angleSetpointFromVel, &velocitySetpoint,
                Kp_vel, Ki_vel, Kd_vel, REVERSE);
```

**Pointer hookup**:
- `*myInput = &velocityInput` — updated from `velocityFeedback`
  (filtered wheel-velocity estimate) just before `Compute()`.
- `*myOutput = &angleSetpointFromVel` — used as the feed-forward
  angle offset added to `baseSetpoint` to produce the balance
  setpoint. Slew-limited and bounded to `±VELOCITY_OUTPUT_MAX`.
- `*mySetpoint = &velocitySetpoint` — CLI / GUI-controlled.

**Direction: REVERSE.** Justification documented in-code and proven
in 4-2: if the bot is moving too fast forward (positive velocity
error against a zero setpoint), we need to lean backward slightly
(negative angle offset) to decelerate. REVERSE flips the sign so the
PID produces the correct-sign angle offset. Verified by the
once-per-sign-iteration `SIGN VERIFY:` debug line already in the
firmware.

**Output clamp**:
`SetOutputLimits(-VELOCITY_OUTPUT_MAX, +VELOCITY_OUTPUT_MAX)` where
`VELOCITY_OUTPUT_MAX` is the max allowable angle offset (a few
degrees). Library clamps `outputSum` and `output` separately.

**outputSum reachability**: same analysis as balancePID. `Ki_vel`
may be nonzero in future tuning, but 4-8 is scoped for the
`Ki == 0` failure mode; nonzero `Ki` would let `outputSum` evolve
naturally regardless of the latching bug.

**Rearm discipline**: every `SetMode(AUTOMATIC)` goes through
`rearmVelocityPID()` (deadband-exit, main-loop arm, CLI `v` toggle,
setup-time init). Verified by grep.

**Deadband-thrashing fix**: the `deadbandActive` state flag from
4-2 prevents MANUAL↔AUTOMATIC flapping inside the deadband region.
Without it, every sample near zero velocity error would toggle the
mode and re-latch a stale output. 4-8's rearm helper reinforces
this by making each rearm clean even if flapping reappears.

## 3. yawPID (yaw correction, 500 Hz, disabled by default)

**Instance**:
```
PID yawPID(&yawInput, &yawOutput, &yawSetpoint, Kp_yaw, Ki_yaw, Kd_yaw, DIRECT);
```

**Pointer hookup**:
- `*myInput = &yawInput` — updated from `yaw` just before
  `Compute()` in the ISR.
- `*myOutput = &yawOutput` — read by ISR, added as differential to
  the left/right motor commands.
- `*mySetpoint = &yawSetpoint` — snapped to current `yaw` on first
  ISR iteration (via `yawSetpointInitialized`) and by
  `rearmYawPID()` on every subsequent arm.

**Direction: DIRECT.** Held as a placeholder — yaw control is
`yawControlEnabled = false` by default, so this loop runs
`Compute()` but its output is ignored until balance is robust.

**Output clamp**: `SetOutputLimits(-maxCurrent, +maxCurrent)`.

**outputSum reachability**: same analysis. `Kp_yaw`, `Ki_yaw`,
`Kd_yaw` all start at zero. `outputSum` is whatever
`rearmYawPID()` produced the last time it was called.

**Rearm discipline**: every `SetMode(AUTOMATIC)` goes through
`rearmYawPID()`. Verified by grep.

## 4. Anti-windup is not the issue here; state-latching was

This is worth spelling out because the firmware has a rich history
of suspecting integral windup whenever the output behaves oddly.
With `Ki = 0.0` across all three PIDs, there is no integrator. The
PIDs are pure PD controllers. And yet until 4-8, every one of them
could exhibit a permanent DC bias on the output that looked exactly
like windup — but originated instead from `Initialize()` snapshotting
a stale `*myOutput` into `outputSum`.

The failure mode and a true windup failure produce the same observed
symptom ("output stuck at one rail with zero error"). The disambiguation:
- A true wound-up integrator would decay as errors of opposite sign
  accumulated. The latching-bug bias did not decay — it persisted
  for > 10 minutes at zero error in the 2026-04-16 screenshot.
- A true wound-up integrator would only appear after sustained
  same-sign error. The latching bug appeared immediately after any
  `MANUAL → AUTOMATIC` transition, regardless of error history.

Fix: `rearmBalancePID()` / `rearmVelocityPID()` / `rearmYawPID()`.
Future consideration: if any `Ki` goes nonzero, revisit this audit
to confirm the integrator behaves and decide whether additional
anti-windup (conditional integration, back-calculation) is needed.
PID_v1's built-in clamping is sufficient for most cases but is the
worst-performing anti-windup method when integrals are large.

## 5. Observable state in the stream

After 4-8, the per-sample stream line carries:
- `R:` raw roll from IMU
- `AI:` EMA-filtered angle fed to `balancePID`
- `RollOut:` latest `motorCurrent` from `balancePID.Compute()`

The triple `R:`, `AI:`, `RollOut:` lets a future session verify
three things simultaneously:
- IMU is live (`R:` changing).
- Filter has settled (`AI - R` small during steady state).
- PID math is sane (`RollOut ≈ Kp · (AI - Setpt)` when `Kd · dAI/dt` is small).

The `GAINS:` line emitted once per second documents which constants
are in effect so a log file is self-describing.
