# I2C Optimization Implementation Guide

**Date**: January 2025  
**Status**: Ready for Testing  
**Target**: 70-85% Reliability (vs 30-40% baseline)

---

## Summary of Changes

### Optimizations Implemented

1. **I2C Bus Speed**: 100kHz → **400kHz** (4x faster)
2. **IMU Update Rate**: 100Hz → **200Hz** (2x faster)
3. **PID Update Rate**: 100Hz → **200Hz** (2x faster)
4. **Velocity Damping**: Kd_vel = 0.0 → **0.15** (eliminates chattering)
5. **Velocity Damping Factor**: 0.01 → **0.02** (adjusted for higher update rate)

### Expected Improvements

- **2x faster sensor updates** (200Hz vs 100Hz)
- **4x faster I2C bus** (400kHz vs 100kHz)
- **Lower latency** (~0.5-1ms vs 1-2ms)
- **Velocity damping enabled** (eliminates motor chattering)
- **Target reliability**: 70-85% (vs 30-40% baseline)

---

## File Created

**New Firmware**: `teensy_balance_logging/teensy_balance_logging_i2c_optimized.ino`

This is the optimized I2C version with all improvements implemented.

---

## Implementation Steps

### Step 1: Verify I2C Wiring

**Current I2C Wiring** (should already be correct):
- VCC (Red) → 3.3V
- GND (Black) → GND
- SCL (Blue) → Pin 19 (Teensy I2C SCL)
- SDA (Purple) → Pin 18 (Teensy I2C SDA)
- PS0 → Floating or GND (I2C mode)
- PS1 → Floating or GND (I2C mode)

**Verify**:
- All connections are secure
- No shorts between wires
- I2C pull-up resistors present (if board doesn't have built-in)

### Step 2: Upload Optimized Firmware

1. Open `teensy_balance_logging/teensy_balance_logging_i2c_optimized.ino` in Arduino IDE
2. Select board: **Teensy 4.1**
3. Upload to Teensy
4. Open Serial Monitor (2000000 baud)

### Step 3: Verify Configuration

**Expected Serial Output**:
```
╔════════════════════════════════════════════════════╗
║     BALANCE ROBOT - OPTIMIZED I2C MODE             ║
╚════════════════════════════════════════════════════╝

🚀 I2C OPTIMIZATION CONFIGURATION:
   • I2C Clock Speed: 400 kHz (Fast Mode)
   • IMU Update Rate: 200 Hz
   • PID Sample Time: 5 ms (200 Hz)

📡 Initializing I2C bus...
   ✓ I2C initialized at 400 kHz

🔧 Initializing IMU...
   ✅ IMU initialized at 0x4A (or 0x4B)
   ✅ Rotation vector enabled at 200 Hz
   ✅ Gyroscope enabled for velocity damping at 200 Hz

⚙️  Initializing VESC motor controllers...
   ✅ VESCs initialized

🎛️  Initializing PID controllers...
   ✅ Angle PID: 200 Hz update rate
   ✅ Velocity PID: 200 Hz update rate (Kd_vel = 0.15)
```

### Step 4: Test Performance

**Baseline Test** (for comparison):
- Record current success rate (should be ~30-40%)
- Note any motor chattering
- Measure typical balance duration

**Optimized Test**:
- Test with optimized firmware
- Record success rate (target: 70-85%)
- Verify motor chattering is eliminated
- Measure typical balance duration

**Test Protocol**:
1. Start robot from rest
2. Attempt to balance for 30+ seconds
3. Record: Success or Failure
4. Repeat 20-50 times
5. Calculate: (Successes / Total) × 100%

---

## Configuration Details

### I2C Settings

```cpp
const uint32_t I2C_CLOCK_SPEED = 400000;  // 400kHz Fast Mode
Wire.setClock(I2C_CLOCK_SPEED);
```

### IMU Settings

```cpp
const uint32_t IMU_UPDATE_RATE_HZ = 200;  // 200Hz
const uint32_t IMU_REPORT_INTERVAL_US = 5000;  // 5ms = 200Hz
bno08x.enableReport(SH2_ROTATION_VECTOR, IMU_REPORT_INTERVAL_US);
bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, IMU_REPORT_INTERVAL_US);
```

### PID Settings

```cpp
const uint32_t PID_SAMPLE_TIME_MS = 5;  // 5ms = 200Hz
anglePID.SetSampleTime(PID_SAMPLE_TIME_MS);
velocityPID.SetSampleTime(PID_SAMPLE_TIME_MS);
```

### Velocity Damping

```cpp
double Kd_vel = 0.15;  // Enabled (was 0.0)
float velocityDamping = 0.02;  // Adjusted for 200Hz (was 0.01 for 100Hz)
```

---

## Troubleshooting

### Issue: I2C Communication Errors

**Symptoms**: IMU fails to initialize, or data is corrupted

**Solutions**:
1. **Check I2C pull-up resistors**: Should be 2.2kΩ-4.7kΩ
2. **Reduce I2C speed**: Try 200kHz instead of 400kHz
3. **Check wire length**: Keep I2C wires short (<6 inches)
4. **Verify wiring**: SCL→Pin 19, SDA→Pin 18

### Issue: IMU Update Rate Not Reaching 200Hz

**Symptoms**: Serial output shows lower actual update rate

**Solutions**:
1. **Check I2C bus speed**: Ensure 400kHz is set
2. **Verify report interval**: Should be 5000 microseconds
3. **Check for I2C errors**: Monitor serial for communication errors
4. **Try slower rate**: If 200Hz fails, try 150Hz (6667 microseconds)

### Issue: Motor Chattering Still Present

**Symptoms**: Motors still chatter despite Kd_vel = 0.15

**Solutions**:
1. **Increase Kd_vel**: Try 0.20 or 0.25
2. **Tune velocity PID**: Adjust Kp_vel and Ki_vel
3. **Check update rate**: Ensure 200Hz is actually running
4. **Verify damping factor**: velocityDamping should be 0.02

---

## Performance Metrics to Track

### Before Optimization (Baseline)
- **Success Rate**: ~30-40%
- **IMU Update Rate**: 100Hz
- **I2C Speed**: 100kHz
- **Motor Chattering**: Present
- **Typical Balance Duration**: 10-30 seconds

### After Optimization (Target)
- **Success Rate**: 70-85%
- **IMU Update Rate**: 200Hz
- **I2C Speed**: 400kHz
- **Motor Chattering**: Eliminated
- **Typical Balance Duration**: 30+ seconds consistently

---

## Next Steps After Testing

### If Target Achieved (70-85% reliability)
1. ✅ **Proceed with Phase 2**: Advanced control (LQR, adaptive control)
2. ✅ **Fine-tune PID gains** for optimized I2C
3. ✅ **Consider sensor fusion** for additional 10-15% gain

### If Target Not Achieved (<70% reliability)
1. **Try 1MHz I2C**: If Teensy supports Fast Mode Plus
2. **Increase IMU rate**: Try 300-400Hz (if I2C bandwidth allows)
3. **Fine-tune PID**: May need adjustment for higher rates
4. **Consider SPI upgrade**: Only if I2C optimization insufficient

---

## Comparison: Baseline vs Optimized

| Metric | Baseline (I2C) | Optimized (I2C) | SPI (Ideal) |
|--------|----------------|-----------------|-------------|
| **I2C Speed** | 100kHz | 400kHz | N/A |
| **IMU Rate** | 100Hz | 200Hz | 1000Hz |
| **PID Rate** | 100Hz | 200Hz | 1000Hz |
| **Latency** | 1-2ms | 0.5-1ms | 0.2-0.5ms |
| **Kd_vel** | 0.0 | 0.15 | 0.15 |
| **Reliability** | 30-40% | 70-85% (target) | 85-95% (target) |
| **Hardware** | Current | Current | New board needed |

---

## Code Changes Summary

### Key Modifications

1. **I2C initialization**:
   ```cpp
   Wire.setClock(400000);  // Was: 100000
   ```

2. **IMU report rate**:
   ```cpp
   bno08x.enableReport(SH2_ROTATION_VECTOR, 5000);  // Was: 10000
   ```

3. **PID sample time**:
   ```cpp
   anglePID.SetSampleTime(5);  // Was: 10
   velocityPID.SetSampleTime(5);  // Was: 10
   ```

4. **Velocity damping**:
   ```cpp
   double Kd_vel = 0.15;  // Was: 0.0
   float velocityDamping = 0.02;  // Was: 0.01
   ```

5. **Roll rate calculation**:
   ```cpp
   rollRate = (roll - lastRoll) * IMU_UPDATE_RATE_HZ;  // Was: * 100.0
   ```

---

## Testing Checklist

- [ ] Upload optimized firmware
- [ ] Verify I2C speed: 400kHz
- [ ] Verify IMU rate: 200Hz
- [ ] Verify PID rate: 200Hz
- [ ] Verify Kd_vel: 0.15
- [ ] Test balance performance
- [ ] Record success rate (target: 70-85%)
- [ ] Verify motor chattering eliminated
- [ ] Compare to baseline metrics

---

**Status**: Ready for testing  
**Expected Results**: 70-85% reliability with optimized I2C  
**Next Phase**: Advanced control (LQR) if target achieved

