# Phase 1: Velocity Control Implementation

## Summary

This document outlines the changes needed to add velocity control to the working single-loop balance controller.

## Key Changes from Single-Loop Version

### 1. Add VESC Encoder Reading
- Read RPM from both VESCs via `getVescValues()`
- Convert RPM to m/s: `velocity = rpm * (wheel_diameter * π / 60)`
- Calculate average velocity: `avgVelocity = (leftVelocity + rightVelocity) / 2.0`
- Add VESC update rate limiting (15ms = 67Hz) to prevent serial buffer overflow

### 2. Add Velocity PID Controller
```cpp
double velocitySetpoint = 0.0;  // Target velocity (m/s)
double velocityInput;            // Current velocity from encoders
double angleSetpointFromVel;     // Output: angle setpoint offset (degrees)

PID velocityPID(&velocityInput, &angleSetpointFromVel, &velocitySetpoint, 
                Kp_vel, Ki_vel, Kd_vel, DIRECT);
```

### 3. Integrate Velocity Loop with Angle Loop
- Angle setpoint = `baseSetpoint + angleSetpointFromVel`
- When `velocitySetpoint = 0.0`, robot balances in place (like single-loop)
- When `velocitySetpoint > 0.0`, robot tilts forward and moves forward
- Velocity loop runs at 100Hz (10ms), slower than angle loop (500Hz)

### 4. Add Velocity Control Commands
- `v` / `V` - Decrease/Increase velocity setpoint
- `0` - Set velocity setpoint to 0.0 (stop and balance)
- `w` / `W` - Decrease/Increase velocity Kp
- `e` / `E` - Decrease/Increase velocity Ki  
- `r` / `R` - Decrease/Increase velocity Kd

### 5. Update Serial Output
- Add velocity setpoint, current velocity, and velocity PID output to data stream
- Update `printTuningValues()` to show velocity PID gains

## Default Tuning Values

### Velocity PID (Start Conservative)
- `Kp_vel = 0.1` (very low, will increase during tuning)
- `Ki_vel = 0.0` (no integral initially)
- `Kd_vel = 0.0` (no derivative initially)

### Angle PID (Use Working Values)
- `Kp = 1.50` (from LAST_WORKING_CONFIG.md)
- `Ki = 0.00`
- `Kd = 0.03`
- `baseSetpoint = -0.70°`

## Testing Strategy

1. **Verify Balance Still Works**
   - Upload firmware
   - Set velocity setpoint to 0.0
   - Robot should balance exactly like single-loop version
   - If balance is broken, check velocity PID integration

2. **Test Velocity Control**
   - Set velocity setpoint to 0.1 m/s (small value)
   - Robot should tilt forward slightly and move forward slowly
   - If robot doesn't move, increase Kp_vel gradually

3. **Tune Velocity PID**
   - Increase Kp_vel until robot responds (target: 0.3-0.5)
   - Add Kd_vel if oscillations occur (target: 0.1-0.2)
   - Add Ki_vel only if steady-state error (target: 0.05-0.1)

## Files Modified

- `teensy_balance_cascaded/teensy_balance_cascaded.ino` - New cascaded control firmware

## Files Unchanged (Fallback)

- `teensy_balance_single_loop/teensy_balance_single_loop.ino` - Working single-loop version (keep as fallback)

