# PBI-4: Actions from Balance Code Review

**Source**: [BALANCE_CODE_REVIEW.md](../../../BALANCE_CODE_REVIEW.md) (root)  
**Date**: 2026-02  
**Note**: SPI migration (Phase 4 in review) is out of scope — board lacks SPI wiring for BNO085. All actions assume I2C @ 400 Hz.

---

## Mapping: Review Issues → Tasks / Actions

| Review ID | Severity | Task / Action | Notes |
|-----------|----------|---------------|--------|
| **CRITICAL-7** | Critical | **4-2** (firmware) | Rate-limit VESC writes to match read rate (~67 Hz). Quick win. |
| **CRITICAL-6** | Critical | **4-2** (firmware) | Increase `VELOCITY_FILTER_ALPHA` (e.g. 0.1 → 0.3). |
| **CRITICAL-2** | Critical | **4-3** | Verify/tune angle PID gains; use EEPROM load and TUNING_RECOMMENDATIONS.md (not necessarily Kp=15 — match your working baseline). |
| **CRITICAL-1, MAJOR-1** | Critical/Major | **New action** | Reset PID state on safety cutoff (SetMode(MANUAL) in cutoff, AUTOMATIC on re-enter). Can be done in 4-2 or small firmware task. |
| **CRITICAL-3** | Critical | Same as C-7 | Rate-limiting motor writes addresses this. |
| **CRITICAL-4** | Critical | **New task or 4-3** | Balance state machine with hysteresis (FALLEN / WARNING / BALANCING / STARTUP). Defer until after velocity sign and Phase 0 fixes. |
| **CRITICAL-5** | Critical | **4-3** | Verify PID_v1 uses derivative-on-measurement; document in 4-3. |
| **MAJOR-2** | Major | **4-3 or new** | Smooth stiction compensation (replace hard 0.55A jump with smooth mapping). |
| **MAJOR-6** | Major | **Firmware** | EEPROM validation for angle PID and maxCurrent in loadSettings(). |
| **MAJOR-3, MAJOR-4, MAJOR-5** | Major | Optional | Variable rename, serial print reduction, motor sign runtime check. |
| **MINOR-1** | Minor | Done | Startup label already fixed to 20 Hz in current firmware. |
| **MINOR-2–5** | Minor | Backlog | Optional cleanups (minCurrent, log buffer, heartbeat, fabs). |

---

## Recommended Order (2–4 h robot time)

1. **Velocity sign** — Ensure `RIGHT_VELOCITY_SIGN` matches VESC (see TUNING_RECOMMENDATIONS.md). Fix VEL SIGN MISMATCH first.
2. **Rate-limit VESC writes** (C-7, C-3) — Single code change, high impact.
3. **Velocity filter alpha** (C-6) — One constant change.
4. **PID reset on safety cutoff** (C-1, MAJOR-1) — Prevent integral windup on recovery.
5. **Verify/tune gains** (C-2, 4-3) — Use log_evaluator and TUNING_RECOMMENDATIONS; match your working baseline, not necessarily review’s Kp=15.
6. **EEPROM validation** (MAJOR-6) — Add range checks for Kp, Ki, Kd, baseSetpoint, maxCurrent in loadSettings().
7. **State machine** (C-4) — After above are stable; implement FALLEN/WARNING/BALANCING/STARTUP with hysteresis.
8. **Smooth stiction** (MAJOR-2) — After balance is stable; reduces chattering.

---

## PRD / Backlog Updates

- **PBI-4 prd**: Reference BALANCE_CODE_REVIEW.md under Technical Approach / Dependencies. No scope change.
- **New tasks**: None required for Phase 0; existing **4-2** and **4-3** cover the bulk. Optionally add a single task “4-2b: Phase 0 firmware fixes (rate-limit writes, filter alpha, PID reset on cutoff)” if you want a separate deliverable; otherwise fold into 4-2 and 4-3.
- **4-3, 4-4, 4-5, 4-6, 4-E2E**: Task files exist or are created; 4-3 explicitly includes review’s Phase 0/1 tuning and gain verification.

---

## Out of Scope (per project decision)

- **SPI migration** (review Phase 4) — Not possible with current BNO085 board wiring.
- **Reliability percentages** in review — Treated as indicative only; focus on “stable balance then add velocity” and 2–4 h test budget.

---

## Next steps (evaluation)

**Immediate (finish 4-2, then Phase 0 firmware)**  
1. **Complete 4-2** — Velocity sign (RIGHT_VELOCITY_SIGN vs VESC), deadband state flag, input clamping. Validate with `log_evaluator.py` on a short stand/floor log; target zero or minimal VEL SIGN MISMATCH when wheels move.  
2. **Phase 0 in firmware** — Rate-limit VESC writes to ~67 Hz; set `VELOCITY_FILTER_ALPHA` to 0.3; reset PID (SetMode(MANUAL) in cutoff, AUTOMATIC on re-enter). These are small code changes with high impact.  
3. **One robot session** — Stand test → floor balance (velocity setpoint 0) → small velocity setpoint. Run log_evaluator on each; document in TUNING_RECOMMENDATIONS or a short baseline note.

**Then (4-3, optional 4-2b)**  
4. **4-3** — Verify/tune angle and velocity gains per TUNING_RECOMMENDATIONS.md; confirm derivative-on-measurement in PID; optionally smooth stiction (MAJOR-2).  
5. **Optional 4-2b** — Only if you want a separate deliverable for "Phase 0 firmware fixes"; otherwise keep them in 4-2/4-3.

**Later (as time allows)**  
6. **MAJOR-6** — EEPROM validation in loadSettings() for gains and maxCurrent.  
7. **4-4** — Sensor fusion only if BNO085 fused output is insufficient.  
8. **4-5** — Kd_vel once velocity loop is stable.  
9. **CRITICAL-4** — State machine (FALLEN/WARNING/BALANCING/STARTUP) after balance is stable.  
10. **4-6 / 4-E2E** — Formal test matrix and E2E CoS sign-off when you're ready to close PBI-4.

**Decision** — No new PBIs or new tasks required. Use 4-2 + 4-3 + BALANCE_REVIEW_ACTIONS order; merge `feature/balance-tuning` to main when 4-2 and Phase 0 are done and you're happy with one good robot session.
