# Baseline Test Protocol - VESCs Powered

**Purpose:** Establish baseline performance metrics with VESCs properly powered  
**Date:** January 2025  
**Status:** Ready to Execute

---

## Pre-Test Checklist

- [ ] VESCs are powered on and operational
- [ ] Teensy is powered via USB
- [ ] IMU is connected and working (I2C)
- [ ] Serial monitor is open and logging
- [ ] Motors are connected to VESCs
- [ ] Safety: Robot is secured/prevented from falling

---

## Test Procedure

### Phase 1: System Initialization (2 minutes)

1. Power on VESCs
2. Power on Teensy (via USB)
3. Wait for system initialization
4. Verify serial output shows:
   - ✅ IMU initialized
   - ✅ VESCs initialized
   - ✅ PID controllers initialized

### Phase 2: Static Test - Motors Off (5 minutes)

1. Keep motors disabled (don't send current commands)
2. Monitor for 5 minutes:
   - IMU communication success rate
   - VESC communication success rate
   - Any Teensy resets
   - Serial output stability

**Expected Results:**
- IMU: >95% success rate
- VESC: >90% success rate (VESCs should respond even with motors off)
- No Teensy resets

### Phase 3: Dynamic Test - Motors Active (5 minutes)

1. Enable motors (send current commands)
2. Try to balance robot (or hold it upright manually)
3. Monitor for 5 minutes:
   - IMU communication success rate
   - VESC communication success rate
   - Any Teensy resets
   - Balance performance (if attempting to balance)
   - Velocity readings (should be reasonable, not extreme)

**Expected Results:**
- IMU: >95% success rate (may drop slightly due to EMI)
- VESC: >90% success rate (may drop slightly due to EMI)
- No Teensy resets
- Velocity readings: Reasonable values (-5 to +5 m/s typical)

### Phase 4: Balance Attempts (10 attempts)

1. Attempt to balance robot 10 times
2. Record success/failure for each attempt
3. Success = Robot balances for 30+ seconds
4. Calculate success rate

**Expected Results:**
- Target: 70-85% success rate
- If <50%: Investigate issues
- If >85%: Excellent, proceed to fine-tuning

---

## Metrics to Record

### Communication Metrics
```
IMU Communication:
  - Success rate: ___%
  - Total reads: ___
  - Failures: ___

VESC Communication:
  - Success rate: ___%
  - Total reads: ___
  - Failures: ___
```

### System Stability
```
Teensy Resets: ___ (should be 0)
Serial Output: Stable / Unstable
```

### Performance Metrics
```
Balance Attempts: 10
Successful: ___
Failed: ___
Success Rate: ___%

Average Balance Duration: ___ seconds
Longest Balance: ___ seconds
```

### Velocity Data Quality
```
Velocity Range: -___ to +___ m/s
Extreme Values: Yes / No
Data Quality: Good / Poor
```

---

## Test Output Format

Record data in this format:

```
=== BASELINE TEST RESULTS ===
Date: ___
Test Duration: ___ minutes

PHASE 1: System Initialization
- IMU Initialized: Yes / No
- VESCs Initialized: Yes / No
- PID Initialized: Yes / No

PHASE 2: Static Test (Motors Off)
- IMU Success Rate: ___%
- VESC Success Rate: ___%
- Teensy Resets: ___

PHASE 3: Dynamic Test (Motors Active)
- IMU Success Rate: ___%
- VESC Success Rate: ___%
- Teensy Resets: ___
- Velocity Range: -___ to +___ m/s

PHASE 4: Balance Attempts
- Total Attempts: 10
- Successful: ___
- Success Rate: ___%
- Average Duration: ___ seconds

ISSUES OBSERVED:
- ___
- ___

NEXT STEPS:
- ___
```

---

## Decision Matrix

### If VESC Communication >90%:
✅ **Proceed to balance testing**
- System is ready for balance attempts
- No hardware fixes needed
- Focus on PID tuning

### If VESC Communication 70-90%:
⚠️ **Monitor closely**
- May need hardware fixes
- Proceed with balance testing but watch for issues
- Consider implementing ferrite beads

### If VESC Communication <70%:
❌ **Implement hardware fixes**
- Ferrite beads (Priority 1)
- Twisted pair wiring (Priority 2)
- Decoupling capacitors (Priority 3)
- Retest after fixes

### If Teensy Resets Occur:
❌ **Investigate immediately**
- Check power supply
- Check grounding
- Check for shorts
- Review code for blocking operations

---

## Notes

- Test in a safe environment (robot secured)
- Have serial monitor open to observe real-time metrics
- Take screenshots of serial output for documentation
- Record any anomalies or unexpected behavior
- If issues occur, note the exact conditions

---

## Post-Test Analysis

After completing the test:

1. **Compare results to expectations**
2. **Identify any issues**
3. **Determine if hardware fixes are needed**
4. **Update EXPERT_REVIEW_DOCUMENT.md with results**
5. **Plan next steps based on results**

---

**Test Status:** Ready to Execute  
**Last Updated:** January 2025





