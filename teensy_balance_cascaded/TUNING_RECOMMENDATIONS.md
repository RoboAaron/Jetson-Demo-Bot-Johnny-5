# Cascaded Balance Tune – What to Change

Tune in this order. Ensure motor/velocity signs in firmware match your VESC configuration (one motor negative so they spin opposite for balance) so the velocity loop gets real wheel feedback.

---

## Plan status (read first)

- **ACTIVE PLAN (latest):** `Current status (validated)` + `Bring-up gameplan (from parity to full cascaded)` in this document.
- **ARCHIVED / SUPERSEDED:** Everything under `Archived plans (historical reference)` is kept for history and comparison, but is not the current step-by-step plan.
- **When in doubt:** Follow the active bring-up gameplan first, then reuse archived details only if needed for targeted troubleshooting.

---

## Current status (validated)

Latest parity log `robot_log_20260311_230518.txt` shows cascaded angle-only behavior close to single-loop when parity controls are active:

- Velocity loop OFF, yaw OFF, VESC feedback bypass active.
- No VEL SIGN MISMATCH and no velocity-noise influence in this mode (`RawVel`/`Vel` stay 0).
- Roll std is low and stable enough for baseline tuning (much closer to single-loop than prior cascaded runs).

This confirms the main instability gap was implementation-path differences, not a fundamental cascaded architecture failure.

---

## Bring-up gameplan (from parity to full cascaded)

Use this sequence to avoid reintroducing multiple variables at once.

1. **Lock parity baseline**
   - Keep `useVelocityLoop = OFF`, yaw OFF.
   - Keep motor output ON and confirm stable 30-60s floor run.
2. **Enable real actuator path only**
   - Keep velocity loop OFF.
   - Keep parity mode ON initially; if stable, test stiction/rate-limit path in short runs.
3. **Enable velocity feedback (still velocity loop OFF)**
   - Disable VESC bypass and verify velocity signs and noise behavior.
   - Do not use VESC success %/Hz from runs where VESC power was toggled during capture.
4. **Enable velocity loop at zero setpoint**
   - `useVelocityLoop = ON`, `VelSet = 0.0`.
   - Verify `angleSetpointFromVel` remains near 0 and no creep appears.
5. **Tune velocity loop incrementally**
   - Start with very small non-zero setpoints.
   - Adjust `Kp_vel` then `Ki_vel` only after inner loop remains stable.
6. **Only then re-enable yaw lock**
   - Keep yaw gains small and verify yaw correction does not disturb balance loop.

---

## What to test now (after latest firmware/GUI changes)

Use this checklist after reflashing and before continuing the bring-up gameplan.

1. **Reflash**
   - Upload `teensy_balance_cascaded/teensy_balance_cascaded.ino` to the Teensy so the robot has: Kd coarse step 0.01 / fine 0.002, Motor Output toggle messages, and any recent fixes.

2. **GUI toggles and status**
   - Connect the tuning GUI. Use **Toggle (v)** (velocity loop), **Toggle (n)** (yaw), **Toggle (o)** (motor output). After each press, the corresponding status label (Velocity Loop / Yaw Control / Motor Output) should update **immediately** to ON/OFF or ENABLED/DISABLED. If a label does not update, re-check serial connection and that you are running the latest GUI.

3. **Kd steps and display**
   - With **Fine Adjust** unchecked: press j/D (decrease/increase Kd). Kd should step by **0.01** and the value should show **3 decimals** (e.g. 0.032, 0.042).
   - Check **Fine Adjust** (or press `t`). Press j/D again. Kd should step by **0.002** (e.g. 0.034, 0.036). Uncheck Fine Adjust and confirm the checkbox state stays in sync with the robot after the next response.

4. **Optional: parity baseline**
   - With velocity loop OFF, yaw OFF, motor output ON: run a short balance (e.g. 30 s), capture a log, and run `log_evaluator.py`. Confirm behavior is still in line with the validated parity run (no new saturation or instability). Then continue from step 2 of the bring-up gameplan when ready.

---

## Archived plans (historical reference)

The sections below were useful during earlier debugging phases, but they are now **superseded** by the active bring-up gameplan above.

---

## 1. Motor direction and velocity sign (match your VESC config)

**VESC configuration:** One motor has a negative sign applied in the VESC so the two motors spin in opposite directions for balance (correct for a differential balance bot). The firmware must match that.

In `teensy_balance_cascaded.ino` (around lines 182–184):

- **`RIGHT_MOTOR_DIRECTION_SIGN`** – Applied to the right motor’s *current* command. Use **-1.0** if the right VESC is configured with a negative (motors spin opposite for balance); use **1.0** if both VESCs have the same sign.
- **`RIGHT_VELOCITY_SIGN`** – Applied to the right wheel’s *reported velocity* so both wheels use the same “forward” convention in software. It should match your VESC setup:
  - If the right VESC already inverts that motor, the right wheel’s reported RPM may already be in the opposite sense; set **`RIGHT_VELOCITY_SIGN = 1.0`** so left and right velocities have the same sign when the robot moves forward.
  - If you see **"VEL SIGN MISMATCH"** in the log (L and R velocities opposite → average forced to 0), flip **`RIGHT_VELOCITY_SIGN`** (e.g. from -1.0 to 1.0), re-flash, and re-test. The velocity loop only works when both wheel velocities are in the same frame.

**Verified (this rig):** Velocity sign is correct for our VESC configuration. No VEL SIGN MISMATCH when wheels are powered and moving. Left/right velocity signs in firmware match motor direction; use log_evaluator on a short run to confirm zero or minimal mismatch count.

Re-flash after any change. Without matching signs, the velocity loop sees zero and tuning it won’t help.

---

## 2. Base angle setpoint

From your log, error went to ~0 when setpoint was about **-1.60°**. That’s your natural lean.

- **Suggested:** Set base setpoint to **-1.60°** or **-1.70°** (GUI: Angle Setpoint ▼/▲, or `z`/`Z`).
- Save with `k` when it feels right.

---

## 3. Angle loop (inner – balance)

Your current values (Kp=1.5, Ki=0, Kd=0.03) are very soft and match the old “last working” single-loop config. To get a better tune:

| Parameter | Current | Try next | Comment |
|-----------|--------|----------|--------|
| **Kp**    | 1.50   | **2.0 → 2.5** | Stronger correction; go up in 0.25 steps. If it oscillates, back off. |
| **Ki**    | 0.00   | **0.05 → 0.10** | Removes steady-state angle error; keep small. |
| **Kd**    | 0.03   | **0.05 → 0.08** | More damping; reduces overshoot and wobble. |

- Tune with **Fine Adjust** on in the GUI (smaller steps).
- **If it’s too soft/slow:** increase Kp (e.g. 2.0, 2.5, 3.0).
- **If it oscillates or jerks:** decrease Kp, increase Kd slightly.
- **If angle drifts from setpoint:** add a bit of Ki (0.05–0.1).

---

## 4. Velocity loop (outer)

Only meaningful when velocity signs match your VESC setup (no VEL SIGN MISMATCH). Right now: Kp_vel=1.0, Ki_vel=0.

| Parameter | Current | Try next | Comment |
|-----------|--------|----------|--------|
| **Kp_vel** | 1.0  | **0.8 – 1.2** | 1.0 is fine to start; if response is twitchy, try 0.8. |
| **Ki_vel** | 0.0  | **0.01 – 0.02** | Small Ki so velocity setpoint is tracked without big overshoot. |

- Keep **velocity setpoint at 0** while you tune the angle loop.
- Once velocity signs are correct, drive slowly (e.g. 0.1–0.2 m/s) and add Ki_vel in small steps (e.g. 0.01) if it doesn’t hold speed.

---

## 5. Max current

- **4.5–6 A** is a good range (you were at 4.5 in the log).
- **5–5.5 A** is a safe default: enough for corrections, not overly aggressive.
- If the log shows motors **saturating at max current** (Left/Right often at ±maxCurrent) while roll swings widely, **lower max current** (e.g. to 5 A) so the PID works in a more linear range and stops "banging" the limits.
- Use `m`/`M` (or GUI ▼/▲) and save with `k`.

---

## 5b. If you see large oscillation

When roll swings widely (e.g. ±5° to ±20°) and motors hit ±maxCurrent repeatedly:

1. **Add damping first:** Increase **Kd** (e.g. 0.03 → **0.06 or 0.08**). Low Kd is the main cause of overshoot and sustained oscillation.
2. **Reduce max current** temporarily to **5.0 A** so the loop doesn’t saturate as hard; re-evaluate with log_evaluator.
3. **Base setpoint:** If you previously had stable balance near **-1.5° to -1.6°**, try that instead of a more upright setpoint (e.g. -0.7°). A setpoint far from the natural lean can force constant large corrections.
4. **One setpoint per run:** Avoid changing setpoint mid-run when tuning; pick one (e.g. -1.5°) and run a short log, then evaluate.
5. After oscillation is reduced, consider **smoothing stiction** (the ±0.55 A step at small error can cause small limit cycling); see BALANCE_CODE_REVIEW MAJOR-2.

---

## 6. Suggested “next step” values (after signs match VESC)

Use these as a single snapshot to try, then tweak from here:

```
Angle PID:    Kp = 2.0,   Ki = 0.05,  Kd = 0.05
Velocity PID: Kp_vel = 1.0, Ki_vel = 0.01
Base setpoint: -1.60°
Max current:  5.0 A
```

- **Angle:** Slightly stiffer than 1.5/0/0.03, with a bit of Ki and Kd.
- **Velocity:** P-only plus a small Ki once velocity signs match your VESC config.
- **Setpoint / current:** Match what you already found stable.

---

## Order of operations

1. Set **RIGHT_MOTOR_DIRECTION_SIGN** and **RIGHT_VELOCITY_SIGN** to match your VESC (one motor negative for opposite spin). If you see "VEL SIGN MISMATCH", flip **RIGHT_VELOCITY_SIGN** (e.g. -1.0 → 1.0), re-flash.
2. Set **base setpoint** to about **-1.60°** (or -1.70°).
3. Set **max current** to **5.0 A**.
4. Bump **angle Kp** to **2.0** (then 2.25, 2.5 if still smooth).
5. Add **angle Ki = 0.05**, **Kd = 0.05**.
6. After confirming balance is good, add **Ki_vel = 0.01** and test slow forward/back.
7. Save with **`k`** when you’re happy with the behavior.

---

## 6b. A/B isolation strategy (single-loop parity)

If cascaded behaves much worse than single-loop even with velocity and yaw off, isolate algorithm differences first.

1. **Run single-loop baseline** and log 20-40 s (same surface, battery level, and setpoint/current values).
2. **Run cascaded in parity mode**:
   - `useVelocityLoop = OFF`
   - `yawControlEnabled = OFF`
   - same angle gains/setpoint/max current as the single-loop baseline
   - parity path enabled in firmware (`SINGLE_LOOP_PARITY_WHEN_VEL_OFF = true`)
3. **Compare only these metrics** with `log_evaluator.py`:
   - Roll std, Error std
   - saturation percentage at `|RollOut| >= maxCurrent`
   - safety trips / recovery behavior
4. **If parity matches single-loop**: re-enable cascaded features one-by-one in this order:
   - stiction jump path
   - 67 Hz motor-write rate limit
   - velocity loop (still zero setpoint first)
5. **If parity still fails**: treat as inner-loop issue and hold velocity/yaw off until inner-loop stats match single-loop.

This keeps each test focused and avoids mixing multiple causes in one run.

---

## Deadband thrashing (fixed)

When velocity setpoint is 0 and measured velocity is small, we want the velocity PID to *not* drive the robot (angle-from-velocity = 0). The wrong approach is to call `SetMode(MANUAL)` and then `SetMode(AUTOMATIC)` every 20 ms depending on whether we're in deadband — that **thrashes** the PID (integral resets, mode toggling).

**Fix:** Use a **state flag** so we transition only once when entering and once when exiting deadband.

1. **Enter deadband once:** When `setpoint ≈ 0` and `|filteredVelocity| < VELOCITY_DEADBAND` (0.08 m/s), set `deadbandActive = true`, `velocityPID.SetMode(MANUAL)`, and `angleSetpointFromVel = 0`. Do not call `SetMode(AUTOMATIC)` again until we leave deadband.
2. **Stay in deadband:** While in deadband, leave output at zero; do not recompute PID.
3. **Exit deadband once:** When setpoint is non-zero or velocity is outside deadband, set `deadbandActive = false` and `velocityPID.SetMode(AUTOMATIC)` once, then run PID normally.

Implemented in `teensy_balance_cascaded.ino` with `static bool deadbandActive`; see velocity PID block (deadband logic) around the 20 Hz update.

---

## Quick reference (GUI / serial)

| Action           | GUI        | Serial |
|------------------|------------|--------|
| Angle Kp         | ▼/▲ on Kp  | p / P  |
| Angle Ki         | ▼/▲ on Ki  | i / I  |
| Angle Kd         | ▼/▲ on Kd  | j / D  |
| Velocity Kp      | ▼/▲ on Vel Kp | w / W |
| Velocity Ki      | ▼/▲ on Vel Ki | e / E |
| Angle setpoint   | ▼/▲ on Angle Setpoint | z / Z |
| Max current      | ▼/▲ on Max Current | m / M |
| Save to EEPROM   | Save Settings | k   |
| Fine steps       | Fine Adjust checkbox | t   |
