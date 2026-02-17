# Cascaded Control Firmware - Phase 1: Velocity Control

## Overview

This firmware implements **Phase 1** of the Position and Velocity Control Plan: adding velocity control loop to enable smooth forward/backward movement while maintaining balance.

## Architecture

### Control Flow
```
Velocity Setpoint → Velocity PID → Angle Setpoint Offset
                                              ↓
Angle Setpoint (base + offset) → Angle PID → Motor Current
```

### Key Features
- **Velocity Control Loop**: Maintains desired velocity (m/s) via encoder feedback
- **Angle Control Loop**: Maintains balance (inherited from single-loop version)
- **Smooth Integration**: Velocity loop output adjusts angle setpoint, enabling controlled movement
- **Fallback Mode**: Can disable velocity loop to return to balance-only mode

## Current Status

**Phase 1: Velocity Control** - ✅ Ready for Implementation

## Usage

1. **Upload this firmware** to Teensy 4.1
2. **Start with velocity setpoint = 0.0** (balances in place)
3. **Gradually increase velocity setpoint** to test forward/backward movement
4. **Tune velocity PID** starting with very low gains (Kp_vel = 0.1)

## Tuning Strategy

### Step 1: Verify Balance Still Works
- With velocity setpoint = 0.0, robot should balance exactly like single-loop version
- If balance is broken, check velocity PID integration

### Step 2: Enable Velocity Control
- Set velocity setpoint to small value (0.1 m/s)
- Start with Kp_vel = 0.1, Ki_vel = 0.0, Kd_vel = 0.0
- Robot should tilt forward slightly and move forward slowly

### Step 3: Tune Velocity PID
- Increase Kp_vel until robot responds to velocity commands (target: 0.3-0.5)
- Add Kd_vel for stability if oscillations occur (target: 0.1-0.2)
- Add Ki_vel only if steady-state error persists (target: 0.05-0.1)

## Commands

### Velocity Control
- `v` / `V` - Decrease/Increase velocity setpoint
- `0` - Set velocity setpoint to 0.0 (stop and balance)

### Velocity PID Tuning
- `w` / `W` - Decrease/Increase velocity Kp
- `e` / `E` - Decrease/Increase velocity Ki
- `r` / `R` - Decrease/Increase velocity Kd

### Balance PID Tuning (unchanged)
- `p` / `P` - Decrease/Increase angle Kp
- `i` / `I` - Decrease/Increase angle Ki
- `j` / `J` - Decrease angle Kd
- `D` - Increase angle Kd

## Next Steps

After Phase 1 is stable:
- **Phase 2**: Add position control loop
- **Phase 3**: Jetson integration for autonomous commands

See `../teensy_balance_single_loop/POSITION_VELOCITY_CONTROL_PLAN.md` for full implementation plan.

