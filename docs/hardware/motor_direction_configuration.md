# Motor Direction Configuration Guide

## Overview

This document explains how to configure motor direction compensation when motors are inverted in VESC configuration or due to wiring differences.

## Problem Statement

When one or both motors are inverted (either in VESC configuration or due to physical wiring), the robot may exhibit unwanted behavior:
- **Turning instead of balancing**: One motor pushes forward while the other pushes backward
- **Velocity sign mismatch**: Encoder readings show opposite signs for motors that should be spinning the same direction
- **Yaw drift**: Unwanted rotation due to motor asymmetry

## Motor Direction Signs

The firmware uses three types of direction signs:

### 1. Motor Current Direction Signs
- **`LEFT_MOTOR_DIRECTION_SIGN`**: Compensation for left motor current commands
- **`RIGHT_MOTOR_DIRECTION_SIGN`**: Compensation for right motor current commands
- **Values**: `1.0` (normal) or `-1.0` (inverted)

**When to invert:**
- If positive current command makes the motor spin backward (from robot's perspective)
- If the motor is inverted in VESC configuration
- If motor wiring is reversed

### 2. Velocity Reading Signs
- **`LEFT_VELOCITY_SIGN`**: Compensation for left motor encoder velocity readings
- **`RIGHT_VELOCITY_SIGN`**: Compensation for right motor encoder velocity readings
- **Values**: `1.0` (normal) or `-1.0` (inverted)

**When to invert:**
- If encoder reports negative RPM when motor spins forward (from robot's perspective)
- **Must match motor direction sign**: If `LEFT_MOTOR_DIRECTION_SIGN = -1.0`, then `LEFT_VELOCITY_SIGN = -1.0` (same for right)

### 3. Relationship Between Signs

**Critical Rule**: Motor direction sign and velocity sign must match for each motor.

```
If LEFT_MOTOR_DIRECTION_SIGN = -1.0 (motor inverted)
Then LEFT_VELOCITY_SIGN = -1.0 (velocity reading also inverted)
```

**Why?** If you invert the motor current command, you must also invert the velocity reading to maintain correct control loop behavior.

## Configuration Procedure

### Step 1: Identify Motor Inversion

1. **Test with diagnostic mode** or manual current commands
2. **Send positive current** to each motor individually
3. **Observe wheel direction** from robot's perspective:
   - Forward = wheel spins to move robot forward
   - Backward = wheel spins to move robot backward

### Step 2: Configure Motor Direction Signs

In `teensy_balance_cascaded.ino`, set:

```cpp
// If left motor is inverted (positive current = backward spin)
const float LEFT_MOTOR_DIRECTION_SIGN = -1.0;
const float RIGHT_MOTOR_DIRECTION_SIGN = 1.0;  // Normal

// If right motor is inverted
const float LEFT_MOTOR_DIRECTION_SIGN = 1.0;   // Normal
const float RIGHT_MOTOR_DIRECTION_SIGN = -1.0;

// If both motors are inverted
const float LEFT_MOTOR_DIRECTION_SIGN = -1.0;
const float RIGHT_MOTOR_DIRECTION_SIGN = -1.0;
```

### Step 3: Configure Velocity Signs

**Match velocity signs to motor direction signs:**

```cpp
// If left motor is inverted, left velocity must also be inverted
const float LEFT_VELOCITY_SIGN = LEFT_MOTOR_DIRECTION_SIGN;  // -1.0 if motor inverted
const float RIGHT_VELOCITY_SIGN = RIGHT_MOTOR_DIRECTION_SIGN;  // -1.0 if motor inverted
```

### Step 4: Verify Configuration

1. **Balance test**: Robot should balance without turning
2. **Velocity test**: Forward velocity command should move robot forward (not turn)
3. **Yaw test**: Robot should maintain heading without unwanted rotation

## Control Loop Impact

### Balance Control (Angle PID)
- Motor direction signs are applied **after** computing balance current
- Ensures both motors push in the same direction to maintain balance

### Velocity Control (Velocity PID)
- Velocity signs ensure correct velocity feedback
- Average velocity calculation uses signed velocities from both motors
- Sign mismatch detection prevents incorrect velocity readings

### Yaw Control (Yaw PID)
- Differential current (yawOutput) is applied to left/right motors
- Motor direction signs ensure yaw correction works correctly

## Example Configuration

**Scenario**: Left motor is inverted in VESC config, right motor is normal.

```cpp
// Motor direction compensation
const float LEFT_MOTOR_DIRECTION_SIGN = -1.0;   // Left motor inverted
const float RIGHT_MOTOR_DIRECTION_SIGN = 1.0;   // Right motor normal

// Velocity reading compensation (must match motor direction)
const float LEFT_VELOCITY_SIGN = -1.0;          // Match left motor
const float RIGHT_VELOCITY_SIGN = 1.0;          // Match right motor
```

**Result**: 
- Positive balance current → both motors push forward (left inverted, right normal)
- Positive velocity setpoint → robot moves forward (both velocities positive after sign compensation)
- Yaw correction → differential current applied correctly to both motors

## Troubleshooting

### Robot Turns Instead of Balancing
- **Symptom**: Robot spins in place when trying to balance
- **Cause**: One motor direction sign is wrong
- **Fix**: Invert the sign for the motor that's pushing backward

### Velocity Sign Mismatch Warnings
- **Symptom**: Frequent "VEL SIGN MISMATCH" warnings in serial output
- **Cause**: Velocity signs don't match motor direction signs
- **Fix**: Ensure `LEFT_VELOCITY_SIGN == LEFT_MOTOR_DIRECTION_SIGN` and same for right

### Robot Moves Backward When Forward Commanded
- **Symptom**: Positive velocity setpoint moves robot backward
- **Cause**: Both motor direction signs are inverted
- **Fix**: Invert both `LEFT_MOTOR_DIRECTION_SIGN` and `RIGHT_MOTOR_DIRECTION_SIGN`

## Best Practices

1. **Always match velocity signs to motor direction signs**
2. **Test each motor individually** before configuring signs
3. **Document your configuration** in code comments
4. **Verify with all control modes**: balance, velocity, and yaw
5. **Use diagnostic mode** to test motor direction without PID control

## Related Files

- `teensy_balance_cascaded/teensy_balance_cascaded.ino` - Main firmware with motor direction constants
- `motor_characterization_test/motor_characterization_test.ino` - Tool for testing individual motors
