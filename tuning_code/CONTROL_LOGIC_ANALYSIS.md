# Control Logic Analysis - Wheels Going Same Direction Issue

## Problem
Wheels go the same direction regardless of positive or negative roll, when they should correct to maintain roll near 0°.

## Current Settings (from GUI)
- Roll: 91.80° (very high - robot likely not upright or orientation issue)
- Balance: BLOCKED (roll > 25° safety limit)
- Angle PID: Kp=3.00, Ki=0.10, Kd=0.10
- Velocity PID: Kp=0.20, Ki=0.05, Kd=0.00
- Angle Setpoint: 0.2°
- Max Current: 7.0A

## Control Flow Analysis

### Expected Behavior
1. **Roll > 0° (tilted forward)**: Robot should move FORWARD (positive velocity) to catch itself
2. **Roll < 0° (tilted backward)**: Robot should move BACKWARD (negative velocity) to catch itself
3. **Roll = 0° (upright)**: Robot should maintain position (zero velocity)

### Current Code Logic

**Line 452-454:**
```cpp
float rollForPID = rollSignInverted ? -roll : roll;
angleInput = rollForPID + (rollRate * velocityDamping);
anglePID.Compute();  // Outputs velocitySetpoint
```

**Line 481-487 (Motor Output):**
```cpp
if (motorDirectionsSwapped) {
  vescLeft.setCurrent(motorOutput);   // Left: +motorOutput
  vescRight.setCurrent(-motorOutput); // Right: -motorOutput
} else {
  vescLeft.setCurrent(-motorOutput); // Left: -motorOutput
  vescRight.setCurrent(motorOutput);  // Right: +motorOutput
}
```

**Current Settings:**
- `motorDirectionsSwapped = true`
- `rollSignInverted = false`
- PID mode: `DIRECT`

## Potential Issues

### Issue 1: Roll Sign Inversion
If wheels go the same direction regardless of roll sign, the PID might be outputting the wrong sign.

**Test:** Toggle `rollSignInverted` (press `Y` in serial monitor or use GUI)

### Issue 2: PID Mode
The PID is set to `DIRECT` mode. If the control response is inverted, it should be `REVERSE`.

**Current:** `PID anglePID(..., DIRECT);`
**If inverted:** Should be `REVERSE`

### Issue 3: Motor Direction Logic
With `motorDirectionsSwapped = true`:
- Left motor: `+motorOutput`
- Right motor: `-motorOutput`

If `motorOutput` is always positive (or always negative), both motors would go the same direction.

**Check:** Is `currentOutput` (which becomes `motorOutput`) always the same sign?

### Issue 4: Roll Value Issue
Roll showing 91.80° suggests:
1. Robot is not upright (tilted 90°)
2. OR IMU orientation correction is wrong
3. OR robot is actually lying down

If roll is always > 25°, motors are disabled by safety check (line 371).

## Diagnostic Steps

1. **Check if motors are disabled:**
   - If roll > 25°, motors are disabled (safety check)
   - GUI shows "Balance: BLOCKED" - this confirms motors are off

2. **Check roll value:**
   - Roll should be near 0° when robot is upright
   - 91.80° suggests robot is tilted or orientation is wrong

3. **Test with robot upright:**
   - Manually hold robot upright (roll near 0°)
   - Check if motors respond correctly to tilting

4. **Check PID output sign:**
   - Monitor `velocitySetpoint` and `currentOutput` in serial output
   - Should change sign when roll changes sign

## Recommended Fixes

### Fix 1: Toggle Roll Sign Inversion
In GUI or serial monitor, press `Y` to toggle `rollSignInverted`.

### Fix 2: Check Roll Orientation
If roll is 91.80° when robot is upright, the IMU orientation correction might be wrong.

### Fix 3: Verify Motor Wiring
If both motors physically go the same direction, check:
- Motor wiring
- VESC configuration
- Motor direction settings

### Fix 4: Test PID Response
1. Hold robot upright (roll = 0°)
2. Tilt forward (roll = +10°)
3. Motors should move forward (both wheels forward)
4. Tilt backward (roll = -10°)
5. Motors should move backward (both wheels backward)

If motors don't respond correctly, toggle `rollSignInverted` or check PID mode.


