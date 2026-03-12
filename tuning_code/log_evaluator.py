#!/usr/bin/env python3
"""
Log evaluator for robot tuning logs (robot_log_YYYYMMDD_HHMMSS.txt).
Parses serial log format and prints a tuning summary: raw and disturbance-filtered
stats, velocity sign mismatches, roll/error distribution, and last SYNC parameters.

Handles real test conditions: wheel power toggled on/off (stand), and manual
disturbances (holding mast, pushing). Use "clean" metrics for PID gain decisions.
"""

import argparse
import re
import sys
from pathlib import Path
from collections import defaultdict

# Default sample rate when timestamps missing (Hz)
DEFAULT_HZ = 20.0


def parse_data_line(line: str, timestamp_sec: float | None = None) -> dict | None:
    """Extract R, Err, Setpt, RollOut, Left, Right, Vel from a data line. Returns None if not a data line."""
    m = re.search(
        r"R:([-\d.]+).*?Err:([-\d.]+).*?RollOut:([-\d.]+).*?Left:([-\d.]+).*?Right:([-\d.]+).*?Setpt:([-\d.]+)",
        line,
    )
    if not m:
        return None
    vel_m = re.search(r"Vel:([-\d.]+)", line)
    vel = float(vel_m.group(1)) if vel_m else None
    raw_vel_m = re.search(r"RawVel:([-\d.]+)", line)
    raw_vel = float(raw_vel_m.group(1)) if raw_vel_m else None
    vel_set_m = re.search(r"VelSet:([-\d.]+)", line)
    vel_setpt = float(vel_set_m.group(1)) if vel_set_m else None
    try:
        row = {
            "roll": float(m.group(1)),
            "err": float(m.group(2)),
            "roll_out": float(m.group(3)),
            "left": float(m.group(4)),
            "right": float(m.group(5)),
            "setpt": float(m.group(6)),
            "vel": vel,
            "raw_vel": raw_vel,
            "vel_setpt": vel_setpt,
        }
        if timestamp_sec is not None:
            row["t"] = timestamp_sec
        return row
    except ValueError:
        return None


def parse_timestamp(line: str) -> float | None:
    """Parse [HH:MM:SS.mmm] at start of line; return seconds since midnight."""
    m = re.match(r"\[(\d{2}):(\d{2}):(\d{2})\.(\d{3})\]", line)
    if not m:
        return None
    h, mi, s, ms = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))
    return h * 3600 + mi * 60 + s + ms / 1000.0


def parse_sync(line: str) -> dict | None:
    """Parse SYNC:... line into key=value dict."""
    if "SYNC:" not in line:
        return None
    part = line.split("SYNC:", 1)[-1].strip()
    out = {}
    for item in part.split(","):
        if "=" in item:
            k, v = item.split("=", 1)
            try:
                out[k.strip()] = float(v.strip())
            except ValueError:
                out[k.strip()] = v.strip()
    return out if out else None


def mean(x):
    return sum(x) / len(x) if x else 0


def std(x):
    if len(x) < 2:
        return 0
    m = mean(x)
    return (sum((v - m) ** 2 for v in x) / (len(x) - 1)) ** 0.5


def print_stats(label: str, rolls, errs, roll_outs, lefts, rights):
    """Print mean/std/min/max for roll, err, roll_out, left, right."""
    print(f"\n{label}")
    print(f"  Roll (°):      mean={mean(rolls):.3f}  std={std(rolls):.3f}  min={min(rolls):.3f}  max={max(rolls):.3f}")
    print(f"  Error (°):     mean={mean(errs):.3f}  std={std(errs):.3f}  min={min(errs):.3f}  max={max(errs):.3f}")
    print(f"  RollOut (A):   mean={mean(roll_outs):.3f}  std={std(roll_outs):.3f}  min={min(roll_outs):.3f}  max={max(roll_outs):.3f}")
    print(f"  Left (A):      mean={mean(lefts):.3f}  std={std(lefts):.3f}  min={min(lefts):.3f}  max={max(lefts):.3f}")
    print(f"  Right (A):     mean={mean(rights):.3f}  std={std(rights):.3f}  min={min(rights):.3f}  max={max(rights):.3f}")


def main():
    p = argparse.ArgumentParser(
        description="Evaluate robot tuning logs. Use clean metrics for PID gain decisions."
    )
    p.add_argument("log_path", type=Path, help="Path to robot_log_*.txt")
    p.add_argument(
        "--disturbance-roll-rate",
        type=float,
        default=30.0,
        help="Roll rate threshold (deg/s) above which row is considered disturbance (default: 30)",
    )
    p.add_argument(
        "--motor-off-current",
        type=float,
        default=0.2,
        help="Current (A) below which motor is considered off (default: 0.2)",
    )
    p.add_argument(
        "--motor-off-error",
        type=float,
        default=1.0,
        help="|Error| (deg) below which row is considered motor-off when current is low (default: 1.0)",
    )
    args = p.parse_args()
    path = args.log_path
    if not path.exists():
        # Try logs/ next to this script (so filename-only works from repo root or tuning_code/)
        script_dir = Path(__file__).resolve().parent
        fallback = script_dir / "logs" / path.name
        if path.name.startswith("robot_log") and fallback.exists():
            path = fallback
        else:
            print(f"File not found: {path}")
            if not path.is_absolute():
                print("  From repo root: python3 tuning_code/log_evaluator.py tuning_code/logs/<filename>")
                print("  From tuning_code/: python3 log_evaluator.py logs/<filename>")
            sys.exit(1)

    data_rows = []
    vel_mismatches = 0
    syncs = []
    i2c_stats = []
    vesc_stats = []
    setpoints_seen = set()

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rest = re.sub(r"^\[\d{2}:\d{2}:\d{2}\.\d{3}\]\s*", "", line)
            ts = parse_timestamp(line)
            if "R:" in rest and "Err:" in rest:
                row = parse_data_line(rest, ts)
                if row:
                    data_rows.append(row)
                    setpoints_seen.add(row["setpt"])
            if "VEL SIGN MISMATCH" in line:
                vel_mismatches += 1
            sync = parse_sync(line)
            if sync:
                syncs.append(sync)
            i2c = re.search(r"I2C Stats: Success=(\d+).*?\(([\d.]+)%\).*?Fail=(\d+).*?Total=(\d+)", line)
            if i2c:
                i2c_stats.append((int(i2c.group(1)), float(i2c.group(2)), int(i2c.group(3)), int(i2c.group(4))))
            vesc = re.search(r"VESC Stats: Success=(\d+).*?\(([\d.]+)%\).*?Fail=(\d+).*?(?:Rate=([\d.-]+) Hz|inactive)", line)
            if vesc:
                rate = vesc.group(4) if vesc.lastindex >= 4 and vesc.group(4) else None
                try:
                    rate_f = float(rate) if rate and rate != "--" else None
                except ValueError:
                    rate_f = None
                vesc_stats.append((int(vesc.group(1)), float(vesc.group(2)), int(vesc.group(3)), rate_f))

    # --- Report header ---
    print("=" * 60)
    print(f"LOG EVALUATOR: {path.name}")
    print("=" * 60)

    if not data_rows:
        print("No data rows parsed. Check log format.")
        sys.exit(0)

    n = len(data_rows)
    dt_default = 1.0 / DEFAULT_HZ

    # Classify rows: motor-off and disturbance
    motor_off = [False] * n
    disturbance = [False] * n
    for i, r in enumerate(data_rows):
        left_abs = abs(r["left"])
        right_abs = abs(r["right"])
        roll_out_abs = abs(r["roll_out"])
        err_abs = abs(r["err"])
        if (left_abs < args.motor_off_current and right_abs < args.motor_off_current
                and roll_out_abs < args.motor_off_current and err_abs < args.motor_off_error):
            motor_off[i] = True
    for i in range(1, n):
        r0, r1 = data_rows[i - 1], data_rows[i]
        t0 = r0.get("t")
        t1 = r1.get("t")
        if t0 is not None and t1 is not None and t1 > t0:
            dt = t1 - t0
        else:
            dt = dt_default
        roll_rate = abs(r1["roll"] - r0["roll"]) / dt
        if roll_rate >= args.disturbance_roll_rate:
            disturbance[i] = True

    clean_mask = [not (motor_off[i] or disturbance[i]) for i in range(n)]
    n_clean = sum(clean_mask)
    n_motor_off = sum(motor_off)
    n_disturbance = sum(disturbance)
    clean_ratio = n_clean / n if n else 0

    print(f"\nData rows:     {n}")
    print(f"Setpoints:     {sorted(setpoints_seen)}")
    print(f"  Excluded:     motor_off={n_motor_off}  disturbance={n_disturbance}  -> clean={n_clean} ({100 * clean_ratio:.1f}%)")
    if clean_ratio < 0.5 and n_clean < n:
        print("  WARNING: Clean data < 50%. Prefer a dedicated clean run (wheels on, minimal push/hold) for gain changes.")

    rolls = [r["roll"] for r in data_rows]
    errs = [r["err"] for r in data_rows]
    roll_outs = [r["roll_out"] for r in data_rows]
    lefts = [r["left"] for r in data_rows]
    rights = [r["right"] for r in data_rows]
    print_stats("All rows (raw)", rolls, errs, roll_outs, lefts, rights)

    if n_clean > 0:
        rolls_c = [data_rows[i]["roll"] for i in range(n) if clean_mask[i]]
        errs_c = [data_rows[i]["err"] for i in range(n) if clean_mask[i]]
        ro_c = [data_rows[i]["roll_out"] for i in range(n) if clean_mask[i]]
        lefts_c = [data_rows[i]["left"] for i in range(n) if clean_mask[i]]
        rights_c = [data_rows[i]["right"] for i in range(n) if clean_mask[i]]
        print_stats("Clean rows (excl. motor-off & disturbance) — use for PID decisions", rolls_c, errs_c, ro_c, lefts_c, rights_c)
    else:
        print("\nClean rows: none (all excluded). Run with wheels powered and minimal manual input for tuning.")

    # --- Raw vs filtered velocity (cascaded logs with RawVel) ---
    vel_rows = [r for r in data_rows if r.get("raw_vel") is not None and r.get("vel") is not None]
    if vel_rows:
        vels = [r["vel"] for r in vel_rows]
        raw_vels = [r["raw_vel"] for r in vel_rows]
        print("\nVelocity (cascaded): Raw vs Filtered")
        print(f"  Rows with Vel+RawVel: {len(vel_rows)}")
        print(f"  Raw velocity (m/s):   mean={mean(raw_vels):.4f}  std={std(raw_vels):.4f}  min={min(raw_vels):.4f}  max={max(raw_vels):.4f}")
        print(f"  Filtered (m/s):       mean={mean(vels):.4f}  std={std(vels):.4f}  min={min(vels):.4f}  max={max(vels):.4f}")
        std_raw = std(raw_vels)
        std_fil = std(vels)
        if std_fil > 1e-9:
            print(f"  Noise reduction:      std(raw)/std(filtered) = {std_raw / std_fil:.2f}x")
        standstill = [r for r in vel_rows if r.get("vel_setpt") is not None and abs(r["vel_setpt"]) < 0.01]
        if standstill:
            v_s = [r["vel"] for r in standstill]
            r_s = [r["raw_vel"] for r in standstill]
            print(f"  At standstill (|VelSet|<0.01): {len(standstill)} rows")
            print(f"    Raw:    mean={mean(r_s):.4f}  std={std(r_s):.4f}  (noise)")
            print(f"    Filtered: mean={mean(v_s):.4f}  std={std(v_s):.4f}")
            if std(r_s) > 0.1:
                print("  -> Raw velocity noise at standstill is large; filtering is important for velocity loop.")

    print(f"\nVEL SIGN MISMATCH count: {vel_mismatches}")
    if vel_mismatches > 0:
        print("  -> Velocity loop is forcing avg to 0 when L and R have opposite signs. See TUNING_RECOMMENDATIONS.md (match RIGHT_VELOCITY_SIGN to VESC).")

    if i2c_stats:
        last = i2c_stats[-1]
        print(f"\nI2C (last):    Success={last[0]}  Rate={last[1]:.1f}%  Fail={last[2]}  Total={last[3]}")
    if vesc_stats:
        last = vesc_stats[-1]
        rate_str = f"{last[3]:.1f} Hz" if last[3] is not None else "inactive"
        print(f"VESC (last):   Success={last[0]}  Rate={last[1]:.1f}%  Fail={last[2]}  {rate_str}")
    if i2c_stats or vesc_stats:
        print("  (I2C/VESC stats are cumulative from firmware start; may include time when VESCs were off or log was toggled.)")
        print("  (Do not use VESC % as a reliability indicator—it reflects long periods with VESCs disabled.)")

    if syncs:
        s = syncs[-1]
        print("\nLast SYNC (tuning snapshot):")
        print(f"  Angle:  Kp={s.get('Kp')}  Ki={s.get('Ki')}  Kd={s.get('Kd')}  setpoint={s.get('setpoint')}°")
        print(f"  Vel:    Kp_vel={s.get('Kp_vel')}  Ki_vel={s.get('Ki_vel')}  velSetpoint={s.get('velSetpoint')}")
        print(f"  Motor:  maxCurrent={s.get('maxCurrent')}A  leftMotorSign={s.get('leftMotorSign')}  rightMotorSign={s.get('rightMotorSign')}")
        print(f"  Vel signs: leftVelSign={s.get('leftVelSign')}  rightVelSign={s.get('rightVelSign')}")

    print("\n--- Next steps ---")
    if clean_ratio < 0.5:
        print("1. Run a clean tuning pass (wheels powered consistently, minimal hold/push) and re-evaluate.")
    print("2. Use clean metrics above for PID gain decisions.")
    print("3. Order: verify motor/velocity signs → setpoint & current limits → angle loop → velocity loop (see TUNING_RECOMMENDATIONS.md).")
    print("=" * 60)


if __name__ == "__main__":
    main()
