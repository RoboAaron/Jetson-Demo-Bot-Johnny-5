# Cascaded Balance Tune – What to Change

Use this after fixing the velocity sign so the velocity loop gets real wheel feedback. Tune in this order.

---

## 1. Firmware: Fix velocity sign (do this first)

In `teensy_balance_cascaded.ino` around line 184:

- **Current:** `RIGHT_VELOCITY_SIGN = -1.0`
- **Change to:** `RIGHT_VELOCITY_SIGN = 1.0`

Re-flash. This removes the "VEL SIGN MISMATCH" and lets the velocity loop use real velocity. Without this, the outer loop always sees 0 and tuning it won’t help.

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

Only meaningful after the velocity sign fix. Right now: Kp_vel=1.0, Ki_vel=0.

| Parameter | Current | Try next | Comment |
|-----------|--------|----------|--------|
| **Kp_vel** | 1.0  | **0.8 – 1.2** | 1.0 is fine to start; if response is twitchy, try 0.8. |
| **Ki_vel** | 0.0  | **0.01 – 0.02** | Small Ki so velocity setpoint is tracked without big overshoot. |

- Keep **velocity setpoint at 0** while you tune the angle loop.
- After the sign fix, drive slowly (e.g. 0.1–0.2 m/s) and add Ki_vel in small steps (e.g. 0.01) if it doesn’t hold speed.

---

## 5. Max current

- **4.5–6 A** is a good range (you were at 4.5 in the log).
- **5–5.5 A** is a safe default: enough for corrections, not overly aggressive.
- Use `m`/`M` (or GUI ▼/▲) and save with `k`.

---

## 6. Suggested “next step” values (after velocity sign fix)

Use these as a single snapshot to try, then tweak from here:

```
Angle PID:    Kp = 2.0,   Ki = 0.05,  Kd = 0.05
Velocity PID: Kp_vel = 1.0, Ki_vel = 0.01
Base setpoint: -1.60°
Max current:  5.0 A
```

- **Angle:** Slightly stiffer than 1.5/0/0.03, with a bit of Ki and Kd.
- **Velocity:** P-only plus a small Ki once the sign is fixed.
- **Setpoint / current:** Match what you already found stable.

---

## Order of operations

1. Change **RIGHT_VELOCITY_SIGN** to **1.0**, re-flash.
2. Set **base setpoint** to about **-1.60°** (or -1.70°).
3. Set **max current** to **5.0 A**.
4. Bump **angle Kp** to **2.0** (then 2.25, 2.5 if still smooth).
5. Add **angle Ki = 0.05**, **Kd = 0.05**.
6. After confirming balance is good, add **Ki_vel = 0.01** and test slow forward/back.
7. Save with **`k`** when you’re happy with the behavior.

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
