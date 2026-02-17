# Session Findings – 2026-02-12

Summary of work and findings from the past five prompts (GUI fixes, HiDPI, log analysis, tuning).

---

## 1. GitHub sync and firmware bug fixes

- **Action:** Pulled latest from `origin/feature/spi-migration` (commit f672d65).
- **Firmware fixes in `teensy_balance_cascaded.ino`:**
  - **Yaw PID limits:** `'m'`/`'M'` (max current) now also call `yawPID.SetOutputLimits(-maxCurrent, maxCurrent)` so yaw stays in sync.
  - **Log download:** Download allowed when `loggingEnabled || logIndex > 0 || bufferFull` so data can be downloaded after buffer fills.
  - **Velocity PID label:** Startup text corrected from "100Hz" to "20Hz" (outer loop).

---

## 2. GUI control and display review (max current arrows + full pass)

- **Log analyzed:** `tuning_code/logs/robot_log_20260212_224057.txt`
- **Finding:** Max current arrows were sending correct `'m'`/`'M'`; firmware responded correctly. Display issues came from other GUI bugs.
- **GUI fixes in `robot_tuning_gui.py`:**
  - Mode label always showed "PID": now reads `control_mode` from IMU stream (stores in `imu_data['control_mode']`), so DIAG/PID display is correct.
  - SYNC: added `leftMotorSign`, `rightMotorSign`, `leftVelSign`, `rightVelSign` to key_map so they are parsed.
  - Command names: `'q'`/`'Q'` labels corrected to "Decrease/Increase Min Current" (was "I2C Speed").
  - Regex: added patterns for "Angle Kp:", "Angle Ki:", "Angle Kd:" (printTuningValues colon format).
  - Fine-adjust checkbox: syncs from SYNC `fineAdjust` so it matches firmware after updates.
- **Help text:** "Download log data (when logging enabled)" → "when data available".

---

## 3. HiDPI / Linux scaling (3840×2400, 200% Ubuntu)

- **Problem:** Text and buttons too small on Linux with high-DPI display.
- **Changes in `robot_tuning_gui.py`:**
  - **`detect_scale_factor(root)`:** Uses GDK_SCALE, Xft.dpi (xrdb), tk DPI, and screen size; applies `tk.call('tk', 'scaling', ...)` so widgets scale.
  - **`_setup_scaling()`:** Font sizes from screen resolution (base 11–13 pt), DejaVu Sans, window 75%×80% of screen (capped), centered.
  - **Buttons:** Toolbar and action buttons use `tk.Button` with `button_font`, `padx`/`pady` for consistent size; arrow buttons (▼/▲) enlarged.
  - **Layout:** Consistent padding, LabelFrames 8–10 px, param rows 4 px.
- **Result:** Scale ~2.0× detected, tk scaling ~2.67, font base 13 pt, window 1800×1100 on 3840×2400.

---

## 4. Log analysis – tune and velocity sign (robot_log_20260212_225822.txt)

- **VESC:** User confirmed VESC was power-cycled during testing; low success rate in log was from that, not a persistent hardware issue.
- **Main tune issue – velocity sign mismatch:**
  - Log showed: `VEL SIGN MISMATCH: L=1.8903 m/s, R=-1.7722 m/s -> Avg forced to 0.0`.
  - Left velocity positive, right negative after signs → firmware forces average to 0 → velocity loop never gets real feedback.
  - **Recommendation:** In firmware set `RIGHT_VELOCITY_SIGN = 1.0` (was -1.0). Re-flash and re-test.
- **Fine-adjust checkbox:** Checkbox doesn’t stay ticked after `'t'` because GUI state comes from last SYNC; firmware toggles correctly. Optional: request SYNC after `'t'` or optimistic GUI update.
- **Oscillation at setpoint:** Small flip between ±0.55 A near zero error is stiction limit cycling; can be refined with deadband/stiction tuning later.

---

## 5. Tuning value recommendations

- **Doc added:** `teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md`
- **Order of operations:** (1) Fix `RIGHT_VELOCITY_SIGN = 1.0`, (2) setpoint ≈ -1.60°, max current 5 A, (3) angle Kp 2.0→2.5, (4) angle Ki 0.05, Kd 0.05, (5) Ki_vel 0.01 after velocity sign fixed, (6) save with `k`.
- **Suggested snapshot:** Kp=2.0, Ki=0.05, Kd=0.05; Kp_vel=1.0, Ki_vel=0.01; setpoint -1.60°; max current 5.0 A.

---

## Files changed this session

| File | Changes |
|------|--------|
| `teensy_balance_cascaded/teensy_balance_cascaded.ino` | Yaw limits on m/M, log-download guard, 20Hz label |
| `tuning_code/robot_tuning_gui.py` | Mode label, SYNC keys, command names, Angle Kp/Ki/Kd regex, fine-adjust sync, help text, HiDPI scaling, fonts, buttons, layout |
| `teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md` | New: step-by-step tuning and value table |
| `tuning_code/logs/robot_log_20260212_*.txt` | Session logs (referenced in findings) |
