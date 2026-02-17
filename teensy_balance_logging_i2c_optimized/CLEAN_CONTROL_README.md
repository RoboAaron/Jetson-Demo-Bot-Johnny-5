# Clean Minimal Control System

## What Changed

We've replaced the complex cascaded PID control with a **clean, minimal single-loop PID** that follows industry best practices and your design philosophy.

### Removed Complexity
- ❌ Cascaded PID (Angle → Velocity → Current)
- ❌ Deadband logic (PID handles it naturally)
- ❌ Position feedback (can add later if needed)
- ❌ Velocity damping mixed into angle input
- ❌ Toggle flags for motor directions (now hardcoded constant)
- ❌ VESC velocity feedback dependency

### New Architecture
- ✅ **Single-Loop PID**: Angle → Motor Current
- ✅ **Diagnostic Mode**: Direct angle→current mapping (no PID) for testing
- ✅ **3 Parameters to Tune**: Kp, Ki, Kd (vs 9+ before)
- ✅ **Hardcoded Motor Direction**: Set `MOTOR_DIRECTION_SIGN` once during hardware setup

## Control Flow

```
IMU Roll Angle → [PID] → Motor Current → Motors
```

That's it. Simple, testable, industry-standard.

## How to Use

### Step 1: Verify Hardware (Diagnostic Mode)

1. Upload the code
2. Press `d` to enter **Diagnostic Mode**
3. On the stand, tilt the robot:
   - **Tilt forward** → Wheels should spin **forward** (positive current)
   - **Tilt backward** → Wheels should spin **backward** (negative current)
   - **Upright** → Wheels should **stop**

4. If wheels move in wrong direction:
   - Edit `MOTOR_DIRECTION_SIGN` in code (change from -1.0 to 1.0 or vice versa)
   - Re-upload

### Step 2: Tune PID

1. Press `d` again to exit Diagnostic Mode (enter PID mode)
2. Start with conservative gains (default: Kp=5.0, Ki=0.1, Kd=0.3)
3. Tune using commands:
   - `p`/`P` - Decrease/Increase Kp
   - `i`/`I` - Decrease/Increase Ki
   - `D` - Increase Kd (damping)
   - `z`/`Z` - Adjust angle setpoint
   - `m`/`M` - Adjust max current

### Step 3: Test on Stand

- Robot should hold position when upright
- Small tilts should produce small corrections
- No wild oscillations or chattering

## Tuning Strategy

1. **Start with Kp only** (set Ki=0, Kd=0)
   - Increase Kp until robot responds but doesn't oscillate
   
2. **Add Kd** (damping)
   - Increase Kd to reduce overshoot and oscillation
   
3. **Add small Ki** (if needed)
   - Only if there's steady-state error
   - Keep Ki small to prevent windup

## Commands

- `d` - Toggle Diagnostic Mode
- `p`/`P` - Tune Kp
- `i`/`I` - Tune Ki
- `D` - Increase Kd
- `z`/`Z` - Adjust angle setpoint
- `m`/`M` - Adjust max current
- `x` - Show all tuning values
- `l`/`s` - Start/Stop logging
- `q`/`Q` - Reduce I2C speed (if seeing failures)

## Configuration

Edit these constants in the code:

- `MOTOR_DIRECTION_SIGN`: -1.0 or 1.0 (determine once during hardware test)
- `angleSetpoint`: Target balance angle (default: 0.0°)
- `maxCurrent`: Maximum motor current (default: 6.0A)
- `minCurrent`: Minimum current to overcome friction (default: 0.3A)

## Expected Behavior

- **Diagnostic Mode**: Direct mapping, wheels should follow tilt direction
- **PID Mode**: Smooth corrections, no wild oscillations
- **On Stand**: Robot should hold position when upright

## Next Steps (Only After Basic Control Works)

1. If robot drifts: Add position feedback (integrate velocity)
2. If oscillations at setpoint: Fine-tune Kd or add deadband
3. If response too slow: Increase Kp or reduce PID sample time

But first: **Get basic control working with this simple system.**

