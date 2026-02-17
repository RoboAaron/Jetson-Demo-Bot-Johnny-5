# Single-Loop PID Tuning Guide

## Overview

This guide will help you tune the single-loop PID controller for stable balancing. The goal is to get the robot to hold position when upright on a stand, then eventually balance on the floor.

## Prerequisites

1. ✅ **Diagnostic Mode Works**: Robot moves forward when tilted forward, backward when tilted backward
2. ✅ **Robot Stops When Upright**: In diagnostic mode, wheels stop when robot is upright (within 0.2°)
3. ✅ **GUI Connected**: Can see real-time data and adjust parameters

## Tuning Strategy

### Phase 1: Proportional Only (Kp)

**Goal**: Get the robot to respond to tilts without oscillating

1. **Set Initial Values**:
   - Kp = 5.0
   - Ki = 0.0 (disable integral)
   - Kd = 0.0 (disable derivative)

2. **Test on Stand**:
   - Tilt robot forward → wheels should spin forward
   - Tilt robot backward → wheels should spin backward
   - Release → robot should return to upright (may overshoot)

3. **Adjust Kp**:
   - **Too weak**: Robot doesn't respond quickly enough
     - **Solution**: Increase Kp (try +1.0 at a time)
   - **Too strong**: Robot oscillates or overshoots
     - **Solution**: Decrease Kp (try -0.5 at a time)
   - **Just right**: Robot responds smoothly, minimal overshoot
     - **Target**: Kp between 3.0-8.0 typically

4. **Success Criteria**:
   - Robot responds to tilts
   - Returns to upright position
   - May overshoot slightly (we'll fix with Kd)
   - No wild oscillations

### Phase 2: Add Derivative (Kd)

**Goal**: Reduce overshoot and oscillation

1. **Keep Kp from Phase 1**
2. **Add Kd**:
   - Start with Kd = 0.3
   - Increase gradually: 0.3 → 0.5 → 0.7 → 1.0

3. **Test on Stand**:
   - Tilt and release
   - Robot should return to upright with less overshoot
   - Should be more stable, less "bouncy"

4. **Adjust Kd**:
   - **Too little damping**: Still overshoots or oscillates
     - **Solution**: Increase Kd (use 'D' command, +0.05 increments)
   - **Too much damping**: Robot feels sluggish, slow to respond
     - **Solution**: Decrease Kd (manually edit code, re-upload - no decrease command)
   - **Just right**: Smooth return to upright, minimal overshoot

5. **Success Criteria**:
   - Robot returns to upright smoothly
   - Minimal overshoot (less than 1-2°)
   - No oscillation
   - Responsive to disturbances

### Phase 3: Add Integral (Ki) - Optional

**Goal**: Eliminate steady-state error (if robot drifts from setpoint)

**⚠️ WARNING**: Integral can cause instability. Only add if needed!

1. **Only add Ki if**:
   - Robot has steady-state error (drifts from setpoint)
   - Robot doesn't quite reach upright position
   - Small persistent offset

2. **Add Small Ki**:
   - Start with Ki = 0.05
   - Keep it small! (Ki > 0.2 usually causes problems)

3. **Test Carefully**:
   - Watch for oscillation
   - If robot starts oscillating, reduce Ki immediately
   - If no improvement, set Ki back to 0

4. **Success Criteria**:
   - Robot reaches exact setpoint
   - No drift
   - Still stable (no oscillation)

## Common Issues and Solutions

### Issue: Robot Doesn't Stop When Upright

**Symptoms**: Wheels keep spinning even when robot is upright

**Causes**:
1. **Angle Setpoint Wrong**: Robot's "upright" isn't 0°
   - **Solution**: Adjust angle setpoint (z/Z commands) until robot stops when upright
   - Try: -0.5°, 0.0°, +0.5°, +1.0°

2. **Integral Windup**: Ki is too high
   - **Solution**: Set Ki = 0, test again

3. **Min Current Too High**: Friction threshold preventing stop
   - **Solution**: Check `minCurrent` in code (should be 0.3A)

### Issue: Robot Oscillates Wildly

**Symptoms**: Robot shakes, wheels spin back and forth rapidly

**Causes**:
1. **Kp Too High**: Over-correction
   - **Solution**: Reduce Kp significantly (try half)

2. **Kd Too Low**: Not enough damping
   - **Solution**: Increase Kd

3. **Ki Too High**: Integral causing instability
   - **Solution**: Reduce Ki or set to 0

### Issue: Robot Responds Too Slowly

**Symptoms**: Robot takes a long time to correct when tilted

**Causes**:
1. **Kp Too Low**: Not enough correction
   - **Solution**: Increase Kp gradually

2. **Kd Too High**: Over-damped
   - **Solution**: Reduce Kd (edit code, re-upload)

### Issue: Robot Moves Wrong Direction

**Symptoms**: Tilt forward → wheels spin backward (or vice versa)

**Causes**:
1. **Motor Direction Sign Wrong**: `MOTOR_DIRECTION_SIGN` in code
   - **Solution**: Change from -1.0 to 1.0 (or vice versa) in code, re-upload

## Recommended Starting Values

| Parameter | Start Value | Typical Range |
|-----------|-------------|---------------|
| **Kp** | 5.0 | 3.0 - 8.0 |
| **Ki** | 0.0 | 0.0 - 0.2 (keep small!) |
| **Kd** | 0.3 | 0.2 - 1.0 |
| **Angle Setpoint** | 0.0° | -1.0° to +1.0° (find where robot stops) |
| **Max Current** | 6.0A | 4.0A - 8.0A |

## Tuning Workflow

1. **Start in Diagnostic Mode** (`d` command)
   - Verify hardware works
   - Find angle where robot stops (adjust setpoint)

2. **Switch to PID Mode** (`d` command again)
   - Start with Kp only (Ki=0, Kd=0)
   - Tune Kp until responsive but not oscillating

3. **Add Kd**
   - Increase Kd until overshoot is minimal
   - Robot should return smoothly to upright

4. **Test Stability**
   - Robot should hold position when upright
   - Small disturbances should be corrected smoothly
   - No wild oscillations

5. **Add Ki (Only if needed)**
   - Only if there's steady-state error
   - Keep it very small (0.05-0.1)

## Using the GUI

1. **Connect** to robot
2. **Watch Real-Time Data**:
   - Roll angle should stay near setpoint
   - Current should be small when upright
   - No wild oscillations in graphs

3. **Adjust Parameters**:
   - Use ▼/▲ buttons to adjust
   - Watch graphs update in real-time
   - Make small changes (0.5 for Kp, 0.05 for Ki/Kd)

4. **Test After Each Change**:
   - Tilt robot and release
   - Watch how it responds
   - Adjust based on behavior

## Success Indicators

✅ **Robot on Stand**:
- Holds position when upright
- Smoothly corrects small tilts
- Returns to upright after disturbance
- No wild oscillations

✅ **Ready for Floor Test**:
- Stable on stand for 30+ seconds
- Responds smoothly to tilts
- No chattering or jerking
- Current stays reasonable (< 3A when stable)

## Next Steps After Tuning

Once robot is stable on stand:
1. Test on floor with safety measures
2. Fine-tune if needed
3. Consider adding position feedback if robot drifts

## Troubleshooting

**Still having issues?**
1. Check diagnostic mode works first
2. Verify angle setpoint is correct
3. Start with very conservative values (Kp=3.0, Ki=0, Kd=0.2)
4. Make one change at a time
5. Test after each change

**Remember**: Start simple, add complexity only when needed!

