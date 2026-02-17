# PID Tuning Guide - Reducing Motor Aggressiveness

**Issue:** Motors moving too aggressively  
**Status:** Communication working (98-99% success) - This is a PID tuning issue

---

## Quick Tuning (Using Serial Commands)

### Reduce Angle PID Gains
```
Press 'p' - Reduce Kp_angle (currently 15.0, try 10.0-12.0)
Press 'd' - Reduce Kd_angle (currently 0.8, try 0.5-0.6)
Press 'i' - Reduce Ki_angle (currently 0.5, try 0.3-0.4)
```

### Reduce Velocity PID Gains
```
Press 'P' - Reduce Kp_vel (currently 0.8, try 0.5-0.6)
Press 'D' - Reduce Kd_vel (currently 0.15, keep this - it's good)
Press 'I' - Reduce Ki_vel (currently 0.3, try 0.2)
```

### Reduce Maximum Current
```
Press 'm' - Reduce maxCurrent (currently 8.0A, try 5.0-6.0A)
```

### View Current Settings
```
Press 'x' - Show all current PID settings
```

---

## Recommended Starting Values

**For Less Aggressive Motors:**
```
Angle PID:
  Kp_angle = 10.0  (was 15.0)
  Ki_angle = 0.3   (was 0.5)
  Kd_angle = 0.6   (was 0.8)

Velocity PID:
  Kp_vel = 0.5     (was 0.8)
  Ki_vel = 0.2     (was 0.3)
  Kd_vel = 0.15    (keep - velocity damping is good)

Max Current = 5.0A  (was 8.0A)
```

---

## Tuning Strategy

### Step 1: Reduce Max Current (Safest First Step)
1. Press 'm' several times to reduce maxCurrent to 5.0A
2. Test on stand - motors should be less aggressive
3. If still too aggressive, reduce further to 4.0A

### Step 2: Reduce Angle PID (Main Control)
1. Press 'p' to reduce Kp_angle to 12.0
2. Press 'd' to reduce Kd_angle to 0.6
3. Test on stand
4. If still too aggressive, reduce Kp_angle further to 10.0

### Step 3: Reduce Velocity PID (Fine Tuning)
1. Press 'P' to reduce Kp_vel to 0.6
2. Test on stand
3. If still too aggressive, reduce to 0.5

### Step 4: Test on Floor
1. Once motors are smooth on stand, try on floor
2. Monitor balance performance
3. Adjust gains incrementally based on behavior

---

## What to Watch For

### Too Aggressive (Current Issue)
- Motors jerk or snap
- Robot oscillates wildly
- Hard to control
- **Solution:** Reduce gains (especially Kp_angle and maxCurrent)

### Too Slow (After Reducing)
- Robot responds slowly to tilts
- Takes too long to correct
- May fall before correcting
- **Solution:** Increase gains slightly

### Oscillations
- Robot wobbles back and forth
- Continuous rocking motion
- **Solution:** Reduce Kp_angle, increase Kd_angle

### Drift
- Robot slowly moves in one direction
- Doesn't stay in place
- **Solution:** Adjust angleSetpoint (press 'z'/'Z')

---

## Current Settings (For Reference)

```
Angle PID:
  Kp = 15.0
  Ki = 0.5
  Kd = 0.8

Velocity PID:
  Kp = 0.8
  Ki = 0.3
  Kd = 0.15  (velocity damping - keep this!)

Max Current = 8.0A
Angle Setpoint = 1.1°
```

---

## Testing Protocol

1. **Start with stand test:**
   - Hold robot upright
   - Watch motor behavior
   - Should be smooth, not jerky

2. **Gradually reduce gains:**
   - Start with maxCurrent (safest)
   - Then Kp_angle (biggest impact)
   - Then Kp_vel (fine tuning)

3. **Test on floor:**
   - Once smooth on stand, try floor
   - Start with gentle tilts
   - Monitor balance performance

4. **Fine-tune:**
   - Adjust based on actual balance behavior
   - May need to increase some gains if too slow
   - Goal: Smooth, stable balance

---

## Expected Behavior After Tuning

- **Smooth motor movements** (no jerking)
- **Controlled corrections** (not aggressive)
- **Stable balance** (robot holds position)
- **Responsive but not over-reactive** (quick but smooth)

---

**Note:** Communication is working perfectly (98-99% success). The aggressiveness is purely a PID tuning issue, not a hardware or communication problem.





