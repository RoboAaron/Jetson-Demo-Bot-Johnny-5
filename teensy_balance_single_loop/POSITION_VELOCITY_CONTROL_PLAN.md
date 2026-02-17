# Position and Velocity Control Plan for Self-Balancing Robot

## Executive Summary

This document outlines the plan to evolve the robot from **basic balancing** to **full position and velocity control** while maintaining balance. This is essential for autonomous navigation, following behaviors, and all higher-level autonomy features.

**Current State**: Single-loop PID (Angle → Current) - excellent for balancing in place  
**Target State**: Cascaded control (Position → Velocity → Angle → Current) - enables controlled movement

---

## 1. Control Architecture Evolution

### Current Architecture (Single-Loop)
```
IMU Roll Angle → PID → Motor Current
```
- ✅ **Strengths**: Simple, stable, easy to tune
- ❌ **Limitations**: Cannot control position or velocity independently
- **Use Case**: Balancing in place, resisting disturbances

### Target Architecture (Cascaded Control)
```
Position Setpoint → Position PID → Velocity Setpoint
                                              ↓
Velocity Setpoint → Velocity PID → Angle Setpoint
                                              ↓
Angle Setpoint → Angle PID → Motor Current
```
- ✅ **Strengths**: Full control over position, velocity, and balance
- ✅ **Industry Standard**: Used by Segway, hoverboards, all commercial balance bots
- **Use Case**: Autonomous navigation, following, waypoint tracking

---

## 2. Why Cascaded Control is Necessary

### Problem with Single-Loop for Movement

**Current "Drive Offset" Approach**:
- We tilt the robot forward (change angle setpoint) to make it move
- This works but has limitations:
  - No velocity feedback - robot accelerates until it falls
  - No position control - cannot return to starting point
  - No smooth deceleration - abrupt stops cause instability
  - Cannot maintain constant velocity

**Cascaded Control Solution**:
- **Position Loop**: Maintains desired position (can be 0 for "stay in place")
- **Velocity Loop**: Maintains desired velocity (can be 0 for "stop")
- **Angle Loop**: Maintains balance (always active)

### Real-World Example

**Scenario**: Robot needs to move forward 2 meters, then stop and balance in place.

**With Single-Loop (Current)**:
1. Tilt forward → robot accelerates
2. No feedback → robot keeps accelerating
3. Try to stop → abrupt angle change → robot falls
4. ❌ **Fails**

**With Cascaded Control**:
1. Position setpoint = 2.0m
2. Position PID → velocity setpoint increases
3. Velocity PID → angle setpoint tilts forward
4. Angle PID → motors accelerate
5. As position approaches 2.0m → velocity setpoint decreases
6. Velocity PID → angle setpoint returns to balance
7. Angle PID → motors decelerate smoothly
8. ✅ **Succeeds**

---

## 3. Implementation Plan

### Phase 1: Add Velocity Control Loop (Foundation)

**Goal**: Add velocity feedback and control without breaking current balance

**Steps**:
1. **Read VESC Encoder Data** (Already Available)
   - VESCs provide RPM via `vescLeft.data.rpm` and `vescRight.data.rpm`
   - Convert RPM to m/s: `velocity = rpm * (wheel_diameter * π / 60)`
   - Average left/right: `avgVelocity = (leftVelocity + rightVelocity) / 2.0`

2. **Create Velocity PID Controller**
   ```cpp
   double velocitySetpoint = 0.0;  // Target velocity (m/s)
   double velocityInput;            // Current velocity from encoders
   double angleSetpointFromVel;     // Output: angle setpoint for balance
   
   PID velocityPID(&velocityInput, &angleSetpointFromVel, &velocitySetpoint, 
                   Kp_vel, Ki_vel, Kd_vel, DIRECT);
   ```

3. **Integrate with Angle Loop**
   - Angle PID setpoint = `baseSetpoint + angleSetpointFromVel`
   - When `velocitySetpoint = 0.0`, robot balances in place
   - When `velocitySetpoint > 0.0`, robot moves forward smoothly

4. **Tuning Strategy**
   - Start with velocity PID gains very low (Kp_vel = 0.1)
   - Tune angle PID first (should already be tuned)
   - Gradually increase velocity gains until responsive but stable
   - **Critical**: Velocity loop must be slower than angle loop

**Estimated Time**: 2-3 hours  
**Risk**: Low (can disable velocity loop if issues)

---

### Phase 2: Add Position Control Loop (Full Control)

**Goal**: Enable position hold and waypoint navigation

**Steps**:
1. **Integrate Position from Velocity**
   ```cpp
   double currentPosition = 0.0;     // Integrated from velocity
   double positionSetpoint = 0.0;   // Target position (m)
   
   // Update position each loop
   float dt = (millis() - lastPositionUpdate) / 1000.0;
   currentPosition += avgVelocity * dt;
   ```

2. **Create Position PID Controller**
   ```cpp
   double velocitySetpointFromPos;  // Output: velocity setpoint
   
   PID positionPID(&currentPosition, &velocitySetpointFromPos, &positionSetpoint,
                   Kp_position, Ki_position, Kd_position, DIRECT);
   ```

3. **Integrate with Velocity Loop**
   - Velocity PID setpoint = `velocitySetpointFromPos`
   - When `positionSetpoint = currentPosition`, velocity setpoint = 0
   - Robot automatically returns to position

4. **Tuning Strategy**
   - Start with position PID disabled (Kp_position = 0.0)
   - Tune velocity loop first (Phase 1)
   - Enable position loop with very low gains (Kp_position = 0.1)
   - Gradually increase until responsive but stable
   - **Critical**: Position loop must be slowest (outermost)

**Estimated Time**: 1-2 hours  
**Risk**: Low (can disable position loop if issues)

---

### Phase 3: Command Interface (Jetson Integration)

**Goal**: Allow Jetson to command position/velocity

**Steps**:
1. **Serial Command Protocol**
   ```cpp
   // Commands from Jetson
   "VEL:0.5"    // Set velocity to 0.5 m/s forward
   "VEL:-0.3"   // Set velocity to 0.3 m/s backward
   "VEL:0.0"    // Stop and balance in place
   "POS:2.0"    // Move to position 2.0m from start
   "POS:0.0"   // Return to starting position
   "RESET"     // Reset position counter to 0
   ```

2. **Command Blending**
   - Velocity commands override position commands
   - Position commands active when velocity = 0
   - Emergency stop always available

3. **Status Reporting**
   - Report current position, velocity, angle to Jetson
   - Report control mode (position/velocity/balance-only)

**Estimated Time**: 1-2 hours  
**Risk**: Low

---

### Phase 4: Advanced Features

**Goal**: Smooth acceleration, obstacle avoidance integration

**Steps**:
1. **Velocity Ramping**
   - Limit acceleration/deceleration rates
   - Prevents sudden changes that cause instability

2. **Position Hold with Disturbances**
   - Increase position PID gains when stationary
   - Reduce gains when moving (prevent overshoot)

3. **Integration with Navigation Stack**
   - Nav2 sends waypoints as position commands
   - Obstacle avoidance adjusts velocity setpoint
   - SLAM provides position feedback (if available)

**Estimated Time**: 2-4 hours  
**Risk**: Medium (requires careful tuning)

---

## 4. Tuning Guidelines

### Tuning Order (Critical!)

1. **Angle PID** (Already Done ✅)
   - Must be stable before adding velocity loop
   - Current values: Kp=5.0, Ki=0.1, Kd=0.3

2. **Velocity PID** (Phase 1)
   - Start: Kp_vel = 0.1, Ki_vel = 0.0, Kd_vel = 0.0
   - Increase Kp_vel until responsive (target: 0.3-0.5)
   - Add Kd_vel for stability (target: 0.1-0.2)
   - Add Ki_vel only if steady-state error (target: 0.05-0.1)

3. **Position PID** (Phase 2)
   - Start: Kp_position = 0.1, Ki_position = 0.0, Kd_position = 0.0
   - Increase Kp_position until responsive (target: 0.3-0.5)
   - Add Kd_position for overshoot control (target: 0.05-0.1)
   - Add Ki_position only if needed (target: 0.01-0.02)

### Loop Response Times

- **Angle Loop**: Fastest (500Hz, 2ms) - must respond to disturbances
- **Velocity Loop**: Medium (100Hz, 10ms) - smooth velocity changes
- **Position Loop**: Slowest (20Hz, 50ms) - gradual position corrections

---

## 5. Safety Considerations

### Fallback Modes

1. **Balance-Only Mode**: Disable velocity/position loops, balance in place
2. **Velocity-Only Mode**: Disable position loop, allow velocity control
3. **Emergency Stop**: Immediately set all setpoints to 0, disable motors if tilt > 25°

### Limits

- **Maximum Velocity**: 1.0 m/s (adjustable based on testing)
- **Maximum Position Error**: 5.0m (safety limit)
- **Tilt Safety**: Always active (disable motors if tilt > 25°)

---

## 6. Testing Plan

### Phase 1 Testing (Velocity Control)
- [ ] Robot balances in place with velocity setpoint = 0
- [ ] Robot moves forward smoothly with velocity setpoint = 0.2 m/s
- [ ] Robot stops smoothly when velocity setpoint returns to 0
- [ ] Robot maintains constant velocity on level ground
- [ ] Robot decelerates on incline (gravity compensation if needed)

### Phase 2 Testing (Position Control)
- [ ] Robot returns to position 0.0 when setpoint = 0.0
- [ ] Robot moves to position 1.0m and stops
- [ ] Robot maintains position when pushed (within limits)
- [ ] Robot smoothly transitions from position to velocity commands

### Phase 3 Testing (Jetson Integration)
- [ ] Jetson can command velocity successfully
- [ ] Jetson can command position successfully
- [ ] Emergency stop works from Jetson
- [ ] Status reporting accurate and timely

---

## 7. References and Best Practices

### Industry Standards
- **Segway**: Cascaded control (position → velocity → angle)
- **Hoverboards**: Similar architecture
- **Balance Bots**: All use cascaded control for movement

### Academic References
- Inverted Pendulum Control Theory
- Cascaded PID Control Design
- Self-Balancing Robot Literature (see `BALANCING_ROBOT_LITERATURE_REVIEW.md`)

### Our Previous Work
- `teensy_balance_logging_i2c_optimized.ino`: Had cascaded control (removed for simplification)
- `BALANCING_ROBOT_LITERATURE_REVIEW.md`: Detailed analysis of why cascaded control is needed
- `SOFTWARE_DESIGN_DOCUMENT.md`: Original design with cascaded architecture

---

## 8. Migration Path

### Option A: Gradual Migration (Recommended)
1. Keep current single-loop firmware as `teensy_balance_single_loop.ino`
2. Create new `teensy_balance_cascaded.ino` with full cascaded control
3. Test cascaded version thoroughly
4. Switch to cascaded when stable
5. Keep single-loop as fallback

### Option B: In-Place Evolution
1. Add velocity loop to current firmware
2. Test thoroughly
3. Add position loop
4. Test thoroughly
5. Update GUI and documentation

**Recommendation**: Option A (safer, allows rollback)

---

## 9. GUI Updates Required

### New Controls Needed
- Velocity setpoint input (slider or text field)
- Position setpoint input
- Position/velocity mode toggle
- Current position display
- Current velocity display
- Position PID tuning controls
- Velocity PID tuning controls

### Status Display
- Control mode indicator (Balance/Velocity/Position)
- Active setpoints (angle, velocity, position)
- Current values (angle, velocity, position)

---

## 10. Timeline Estimate

- **Phase 1 (Velocity Control)**: 2-3 hours
- **Phase 2 (Position Control)**: 1-2 hours
- **Phase 3 (Jetson Integration)**: 1-2 hours
- **Phase 4 (Advanced Features)**: 2-4 hours
- **GUI Updates**: 2-3 hours
- **Testing and Tuning**: 4-6 hours

**Total**: 12-20 hours of development + testing

---

## 11. Success Criteria

✅ **Phase 1 Complete When**:
- Robot can balance in place (velocity = 0)
- Robot can move forward/backward at commanded velocity
- Smooth acceleration and deceleration
- No oscillations or instability

✅ **Phase 2 Complete When**:
- Robot can move to commanded position
- Robot returns to position after disturbance
- Smooth transitions between position and velocity modes

✅ **Phase 3 Complete When**:
- Jetson can successfully command robot movement
- All commands work reliably
- Status reporting accurate

✅ **Full System Complete When**:
- Robot can navigate to waypoints autonomously
- Robot maintains balance during all operations
- System is stable and reliable (>99% success rate)

---

## Conclusion

This plan provides a clear, incremental path from basic balancing to full position and velocity control. The cascaded control architecture is the industry standard for a reason - it provides the control authority needed for autonomous navigation while maintaining stability.

**Next Step**: Begin Phase 1 (Velocity Control Loop) after current yaw control is stable and tested.

