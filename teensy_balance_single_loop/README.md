# Single-Loop PID Control Implementation

## Overview

This is a **clean, minimal control implementation** using a single-loop PID architecture (Angle → Motor Current). This follows industry best practices and the project's design philosophy of starting simple and adding complexity only when needed.

## When to Use This vs. Cascaded Control

### Use This (Single-Loop) When:
- ✅ **Getting started** - Need to verify basic control works
- ✅ **Troubleshooting** - Cascaded control has too many variables
- ✅ **Hardware verification** - Diagnostic mode tests sensor→motor causality
- ✅ **Simple tuning** - Only 3 parameters (Kp, Ki, Kd) vs 9+ in cascaded

### Use Cascaded Control (`teensy_balance_logging_i2c_optimized/`) When:
- ✅ Single-loop is stable and working
- ✅ Need position control (prevent drift)
- ✅ Need velocity feedback for smoother operation
- ✅ Ready to add complexity incrementally

## Architecture Comparison

| Aspect | Single-Loop | Cascaded |
|--------|------------|----------|
| **Control Flow** | Angle → Current | Angle → Velocity → Current |
| **Parameters** | 3 (Kp, Ki, Kd) | 9+ (2 PIDs + position + deadband) |
| **Dependencies** | IMU only | IMU + VESC velocity feedback |
| **Complexity** | Low | High |
| **Tuning Difficulty** | Easy | Difficult |
| **Industry Standard** | ✅ Yes (most DIY bots) | Used in industrial systems |

## Key Features

1. **Diagnostic Mode** (`d` command)
   - Direct angle → current mapping (no PID)
   - Verifies: Tilt forward → wheels forward
   - Use this FIRST to verify hardware/wiring

2. **Single-Loop PID**
   - Pure angle control
   - No velocity feedback dependency
   - No deadband logic (PID handles it)

3. **Hardcoded Motor Direction**
   - Set `MOTOR_DIRECTION_SIGN` once during hardware test
   - No runtime toggles to confuse debugging

## Quick Start

1. **Upload firmware** to Teensy
2. **Enter Diagnostic Mode**: Press `d`
3. **Test on stand**:
   - Tilt forward → wheels should spin forward
   - Tilt backward → wheels should spin backward
   - Upright → wheels should stop
4. **If direction wrong**: Edit `MOTOR_DIRECTION_SIGN` in code, re-upload
5. **Exit Diagnostic**: Press `d` again
6. **Tune PID**: Use `p`/`P`, `i`/`I`, `D` commands

## Files

- `teensy_balance_single_loop.ino` - Main firmware
- `../teensy_balance_logging_i2c_optimized/CLEAN_CONTROL_README.md` - Detailed usage guide

## Migration Path

1. **Start here** - Get single-loop working and stable
2. **Verify** - Robot balances on stand, no wild oscillations
3. **Then consider** - Adding velocity cascade or position feedback if needed

## See Also

- `../teensy_balance_logging_i2c_optimized/` - Cascaded control implementation (original)
- `../tuning_code/` - Python GUI for real-time tuning

