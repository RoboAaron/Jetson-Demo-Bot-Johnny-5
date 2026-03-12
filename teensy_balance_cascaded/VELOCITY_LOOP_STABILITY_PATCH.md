# Velocity Loop Stability Patch - Summary

## Status

**ARCHIVED / SUPERSEDED DOCUMENT**

- This patch summary is retained as historical context.
- The current authoritative plan is in `teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md`, specifically:
  - `Current status (validated)`
  - `Bring-up gameplan (from parity to full cascaded)`
- If guidance here conflicts with `TUNING_RECOMMENDATIONS.md`, follow `TUNING_RECOMMENDATIONS.md`.

---

## Changes Implemented

### 1. Velocity PID Update Rate: 20 Hz (50ms)
- Changed `VELOCITY_PID_SAMPLE_TIME_MS` from 10ms (100Hz) to 50ms (20Hz)
- Velocity PID now runs independently of angle loop using timer-based updates
- Prevents velocity loop from interfering with fast angle control

### 2. PI-Only Controller (Kd = 0)
- `Kd_vel` is now **always 0** (enforced in code)
- Removed Kd tuning commands (now shows message that Kd is always 0)
- Prevents derivative noise from causing instability

### 3. EMA Velocity Filter (α = 0.1)
- Added `filteredVelocity` with EMA filter: `vel_filt = 0.1*vel + 0.9*vel_filt`
- Filters noisy encoder velocity measurements
- Reduces velocity PID reaction to noise

### 4. Deadband When VelSet = 0
- When `abs(velocitySetpoint) < 0.01` AND `abs(filteredVelocity) < 0.08 m/s`:
  - Treats velocity error as 0 (sets `velocityInput = velocitySetpoint`)
  - Prevents integrator windup when robot should be stationary
  - Does NOT toggle PID mode (keeps it in AUTOMATIC)

### 5. Output Clamping: ±0.5°
- Velocity PID output clamped to `±VELOCITY_OUTPUT_MAX` (±0.5°)
- Prevents velocity loop from overwhelming angle control
- Can be increased later if needed (currently 0.5°)

### 6. Slew Rate Limiting: 0.05° per update
- `angleSetpointFromVel` can change by maximum `VELOCITY_SLEW_RATE` (0.05°) per 50ms update
- Prevents sudden angle setpoint changes
- Smooths velocity loop response

### 7. Debug Prints at 10 Hz
- Added debug output every 5 velocity PID updates (250ms = 10 Hz)
- Shows: `vel_raw`, `vel_filt`, `vel_err`, `vel_out`, `vel_setpt`, `totalAngleSetpoint`
- Format: `🔍 VEL: raw=0.123 filt=0.120 err=0.120 out=0.045 setpt=0.000 totalSetpt=-0.655`

## Recommended Starting Gains

### Velocity PID (PI Only)
```
Kp_vel = 0.05   (start conservative)
Ki_vel = 0.01   (small integral to eliminate steady-state error)
Kd_vel = 0.00   (ALWAYS 0 - enforced in code)
```

### Angle PID (Use Your Working Values)
```
Kp = 1.50       (from your working single-loop)
Ki = 0.00       (from your working single-loop)
Kd = 0.03       (from your working single-loop)
baseSetpoint = -0.70°  (from your working single-loop)
```

## Tuning Strategy

1. **Start with velocity setpoint = 0.0** (should balance like single-loop)
2. **Verify balance works** with velocity loop active but output near zero
3. **Check debug output** - filtered velocity should be stable near 0.0
4. **Gradually increase Kp_vel** if robot doesn't respond to velocity commands:
   - Increase by 0.01-0.02 at a time
   - Target range: 0.05-0.15
5. **Add Ki_vel** only if steady-state velocity error persists:
   - Start with 0.01, increase slowly
   - Watch for overshoot/oscillations
6. **Test with small velocity setpoint** (0.1 m/s) once balance is stable

## Key Parameters (Adjustable)

```cpp
const float VELOCITY_FILTER_ALPHA = 0.1;      // EMA filter (0.0-1.0, lower = more filtering)
const float VELOCITY_DEADBAND = 0.08;        // Deadband when setpoint = 0 (m/s)
const float VELOCITY_OUTPUT_MAX = 0.5;        // Max output (±degrees)
const float VELOCITY_SLEW_RATE = 0.05;       // Max change per update (degrees)
const uint32_t VELOCITY_PID_SAMPLE_TIME_MS = 50;  // Update rate (20 Hz)
```

## Expected Behavior

### With Velocity Setpoint = 0.0:
- Filtered velocity should stabilize near 0.0 m/s
- Velocity PID output should be near 0.0° (within deadband)
- Robot should balance exactly like single-loop version
- Debug output should show: `err≈0.000 out≈0.000`

### With Velocity Setpoint > 0.0:
- Robot should tilt forward smoothly
- Velocity should gradually approach setpoint
- Angle setpoint offset should change smoothly (slew rate limited)
- No oscillations or overshoot

## Troubleshooting

**If balance is still unstable:**
1. Check debug output - is filtered velocity noisy?
   - If yes, reduce `VELOCITY_FILTER_ALPHA` to 0.05
2. Is velocity PID output oscillating?
   - Reduce `Kp_vel` further (try 0.02-0.03).
3. Is angle setpoint changing too fast?
   - Reduce `VELOCITY_SLEW_RATE` to 0.02-0.03.

**If robot doesn't respond to velocity commands:**
1. Increase `Kp_vel` gradually (0.05 → 0.08 → 0.10).
2. Check if output is hitting clamp limit - may need to increase `VELOCITY_OUTPUT_MAX` slightly (but be careful).

## Code Changes Summary

- Modified velocity PID update to run at 20 Hz (50ms intervals)
- Added EMA filter for velocity
- Added deadband logic for zero setpoint
- Added output clamping and slew rate limiting
- Enforced PI-only controller (Kd always 0)
- Added 10 Hz debug output
- Updated all references to use filtered velocity
