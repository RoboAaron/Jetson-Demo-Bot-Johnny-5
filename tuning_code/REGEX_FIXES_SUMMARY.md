# Regex Variable Overlap Fixes - Summary

## Issues Fixed

### 1. Command Mapping Conflicts

**Fixed:**
- **'r'/'R' conflict**: Was mapped to both "Velocity Kd" and "Reset Yaw Setpoint"
  - **Solution**: Removed "Reset Yaw Setpoint" button (not available in cascaded firmware)
  - **Firmware**: 'r'/'R' shows message that velocity Kd is always 0 (PI only)

- **'V' conflict**: Was mapped to both "Increase Velocity Setpoint" and "Save Settings"
  - **Solution**: Changed "Save Settings" to use 'k'/'K' (matches firmware)
  - **Firmware**: 'V' = velocity setpoint increase, 'k'/'K' = save settings

- **'v' conflict**: Was mapped to both "Decrease Velocity Setpoint" and "Load Settings"
  - **Solution**: Changed "Load Settings" to use 'g'/'G' (matches firmware)
  - **Firmware**: 'v' = velocity setpoint decrease, 'g'/'G' = load settings

- **'b'/'f' conflicts**: Were used for both "Drive Offset" and "Legacy Drive"
  - **Solution**: Removed Drive Offset and Legacy Drive controls (not in cascaded firmware)
  - **Firmware**: Cascaded firmware uses velocity setpoint instead of drive offset

- **'0' conflict**: Used for both velocity stop and drive stop
  - **Solution**: '0' only stops velocity (matches firmware)
  - **Firmware**: '0' = set velocity setpoint to 0.0

### 2. Regex Pattern Overlaps

**Fixed:**
- **Velocity PID patterns** now come FIRST (most specific)
  - `\bVelocity Kp\s*=\s*([\d.]+)` → maps to `Kp_vel`
  - `\bVel Kp\s*=\s*([\d.]+)` → maps to `Kp_vel`
  - Prevents "Velocity Kp" from being parsed as "Roll Kp"

- **Roll PID patterns** come second
  - `\bRoll Kp\s*=\s*([\d.]+)` → maps to `Kp`
  - `\bAngle Kp\s*=\s*([\d.]+)` → maps to `Kp`
  - Prevents "Roll Kp" from being parsed as "Velocity Kp"

- **Yaw PID patterns** come third
  - `\bYaw Kp\s*=\s*([\d.]+)` → maps to `Kp_yaw`
  - Prevents "Yaw Kp" from being parsed as "Roll Kp" or "Velocity Kp"

- **Generic patterns** come last with negative lookbehinds
  - `(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )\bKp\s*=\s*([\d.]+)` → maps to `Kp`
  - Only matches if NOT preceded by "Roll ", "Yaw ", "Velocity ", or "Vel "

### 3. Parameter Key Uniqueness

**Verified Unique Keys:**
- `Kp` - Angle PID proportional gain
- `Ki` - Angle PID integral gain
- `Kd` - Angle PID derivative gain
- `Kp_vel` - Velocity PID proportional gain
- `Ki_vel` - Velocity PID integral gain
- `Kd_vel` - Velocity PID derivative gain (always 0, display only)
- `Kp_yaw` - Yaw PID proportional gain
- `Ki_yaw` - Yaw PID integral gain
- `Kd_yaw` - Yaw PID derivative gain

**No overlaps** - each parameter has a unique key in `param_labels` dictionary.

### 4. Command-to-Parameter Mapping

**Verified Correct Mappings:**
- `'p'/'P'` → Angle Kp (`Kp`)
- `'i'/'I'` → Angle Ki (`Ki`)
- `'j'/'D'` → Angle Kd (`Kd`)
- `'w'/'W'` → Velocity Kp (`Kp_vel`)
- `'e'/'E'` → Velocity Ki (`Ki_vel`)
- `'r'/'R'` → Velocity Kd (disabled, shows message)
- `'y'/'Y'` → Yaw Kp (`Kp_yaw`)
- `'u'/'U'` → Yaw Ki (`Ki_yaw`)
- `'h'/'H'` → Yaw Kd (`Kd_yaw`)

**Each command controls exactly one parameter** - no overlaps.

## Testing Checklist

- [ ] Verify "Vel Kp" button controls `Kp_vel` (not `Kp`)
- [ ] Verify "Kp" button controls `Kp` (not `Kp_vel` or `Kp_yaw`)
- [ ] Verify "Yaw Kp" button controls `Kp_yaw` (not `Kp` or `Kp_vel`)
- [ ] Verify "Save Settings" uses 'k' (not 'V')
- [ ] Verify "Load Settings" uses 'g' (not 'v')
- [ ] Verify "Vel Kd" shows "0.00 (PI only)" and doesn't change
- [ ] Verify regex correctly parses "Velocity Kp = 0.05" → `Kp_vel = 0.05`
- [ ] Verify regex correctly parses "Roll Kp = 1.50" → `Kp = 1.50`
- [ ] Verify regex correctly parses "Yaw Kp = 0.50" → `Kp_yaw = 0.50`

## Regex Pattern Order (Critical!)

The order of patterns in `_process_line()` is critical. Patterns are checked in order, and the FIRST match wins:

1. **Velocity PID patterns** (most specific, checked first)
2. **Roll/Angle PID patterns** (checked second)
3. **Yaw PID patterns** (checked third)
4. **Generic patterns with negative lookbehinds** (checked last, only if no prefix found)

This ensures "Velocity Kp" is never parsed as "Roll Kp" or generic "Kp".
