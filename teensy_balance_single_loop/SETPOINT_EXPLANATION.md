# Angle Setpoint Explanation - Self-Balancing Robot

## Why Your Robot Balances at -2.5° (Not 0°)

### This is **NORMAL and CORRECT** behavior!

## Understanding the Balance Point

### What is the Setpoint?

The **angle setpoint** is the target roll angle where you want the robot to balance. For a two-wheeled self-balancing robot:

- **ROLL angle** (forward/backward tilt) is the correct angle to control ✅
- **PITCH angle** (side-to-side tilt) is typically not used for balance control
- **YAW angle** (rotation) is not used for balance

### Why Not 0°?

Your robot's **natural balance point** is around **-2.5°**, not 0°. This is due to:

1. **Center of Mass Offset**: Battery, electronics, and frame weight distribution
2. **Motor Mounting Angle**: Motors may not be perfectly perpendicular
3. **IMU Mounting Angle**: IMU may be slightly tilted
4. **Mechanical Tolerances**: Frame construction, wheel alignment

### Industry Best Practice

**Standard approach for self-balancing robots:**

1. **Find the natural balance point** (where robot balances with minimal effort)
2. **Set setpoint to that angle** (not necessarily 0°)
3. **Robot will balance at that angle** with minimal drift

## Your Current Implementation

### ✅ CORRECT: Using Roll Angle

```cpp
angleInput = roll;  // Using roll angle for PID control
angleSetpoint = -2.5;  // Set to robot's natural balance point
```

This is the **standard approach** for two-wheeled self-balancing robots.

### Why It Works

- **Roll angle** represents forward/backward tilt
- When robot tilts forward (positive roll), wheels need to move forward
- When robot tilts backward (negative roll), wheels need to move backward
- PID controller corrects roll angle to match setpoint

## Finding Your Balance Point

### Method 1: Manual Tuning (What You're Doing)

1. Start with setpoint = 0.0°
2. Observe where robot naturally wants to balance
3. Adjust setpoint until robot holds position without drifting
4. **Your balance point: ~-2.5°** ✅

### Method 2: Auto-Calibration (Future Enhancement)

1. Robot measures roll angle when motors are off
2. Finds average roll over 5-10 seconds
3. Sets that as the balance point automatically

## Comparison to Best Practices

### ✅ Your Implementation Matches Industry Standards

| Aspect | Industry Standard | Your Implementation | Status |
|--------|------------------|---------------------|--------|
| **Control Angle** | Roll (forward/back) | Roll | ✅ Correct |
| **Setpoint Type** | Natural balance point | Adjustable setpoint | ✅ Correct |
| **PID Architecture** | Single-loop (Angle→Current) | Single-loop | ✅ Correct |
| **Setpoint Value** | Not necessarily 0° | -2.5° (found through tuning) | ✅ Correct |

### Common Self-Balancing Robot Setpoints

- **Segway-style robots**: Often balance at 0° (well-calibrated)
- **DIY robots**: Typically -3° to +3° (mechanical tolerances)
- **Your robot**: -2.5° (within normal range) ✅

## What the Setpoint Does

### When Setpoint = -2.5°:

1. **Robot at -2.5°**: Error = 0°, motors minimal current → **Robot holds position** ✅
2. **Robot at -3.0°**: Error = -0.5°, motors correct backward → **Robot moves forward** ✅
3. **Robot at -2.0°**: Error = +0.5°, motors correct forward → **Robot moves backward** ✅

### When Setpoint = 0.0° (Wrong for Your Robot):

1. **Robot at -2.5°**: Error = -2.5°, motors try to correct → **Robot drifts forward continuously** ❌
2. This is why you had to adjust to -2.5°!

## Recommendations

### ✅ Keep Your Current Setup

- **Setpoint: -2.5°** is correct for your robot
- **Roll angle control** is the right approach
- **No changes needed** - this is working as designed

### Optional: Add Visual Indicator

Consider adding a GUI indicator showing:
- **"Balance Point: -2.5°"** (static, doesn't change)
- **"Current Roll: -2.5°"** (updates in real-time)
- **"Error: 0.0°"** (when balanced)

This helps users understand that -2.5° is the target, not a problem.

## Summary

**Your robot balancing at -2.5° is CORRECT and NORMAL.**

- ✅ Using roll angle is the right approach
- ✅ Setpoint should match natural balance point
- ✅ -2.5° is a reasonable balance point
- ✅ Your implementation follows industry best practices

**No action needed** - your setup is working correctly!

