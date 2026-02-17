#!/usr/bin/env python3
"""
Log evaluator for robot tuning logs (robot_log_YYYYMMDD_HHMMSS.txt).
Parses serial log format and prints a tuning summary: stats, velocity sign
mismatches, roll/error distribution, and last SYNC parameters.
"""

import re
import sys
from pathlib import Path
from collections import defaultdict


def parse_data_line(line: str) -> dict | None:
    """Extract R, Err, Setpt, RollOut, Left, Right, Vel from a data line. Returns None if not a data line."""
    # Log order: R:, P:, Y:, Err:, ... RollOut:, YawOut:, Left:, Right:, Setpt:, ...
    m = re.search(
        r"R:([-\d.]+).*?Err:([-\d.]+).*?RollOut:([-\d.]+).*?Left:([-\d.]+).*?Right:([-\d.]+).*?Setpt:([-\d.]+)",
        line,
    )
    if not m:
        return None
    vel_m = re.search(r"Vel:([-\d.]+)", line)
    vel = float(vel_m.group(1)) if vel_m else None
    try:
        return {
            "roll": float(m.group(1)),
            "err": float(m.group(2)),
            "roll_out": float(m.group(3)),
            "left": float(m.group(4)),
            "right": float(m.group(5)),
            "setpt": float(m.group(6)),
            "vel": vel,
        }
    except ValueError:
        return None


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


def main():
    if len(sys.argv) < 2:
        print("Usage: python log_evaluator.py <path_to_robot_log_*.txt>")
        sys.exit(1)
    path = Path(sys.argv[1])
    if not path.exists():
        print(f"File not found: {path}")
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
            # Data line: optional [HH:MM:SS.mmm] then R:... (or line that contains R: and Err:)
            rest = re.sub(r"^\[\d{2}:\d{2}:\d{2}\.\d{3}\]\s*", "", line)
            if "R:" in rest and "Err:" in rest:
                row = parse_data_line(rest)
                if row:
                    data_rows.append(row)
                    setpoints_seen.add(row["setpt"])
            if "VEL SIGN MISMATCH" in line:
                vel_mismatches += 1
            sync = parse_sync(line)
            if sync:
                syncs.append(sync)
            # I2C Stats: Success=77 (97.5%), Fail=2, Total=79
            i2c = re.search(r"I2C Stats: Success=(\d+).*?\(([\d.]+)%\).*?Fail=(\d+).*?Total=(\d+)", line)
            if i2c:
                i2c_stats.append((int(i2c.group(1)), float(i2c.group(2)), int(i2c.group(3)), int(i2c.group(4))))
            # VESC Stats: Success=0 (0.0%), Fail=156, Rate=-- Hz
            vesc = re.search(r"VESC Stats: Success=(\d+).*?\(([\d.]+)%\).*?Fail=(\d+).*?(?:Rate=([\d.-]+) Hz|inactive)", line)
            if vesc:
                rate = vesc.group(4) if vesc.lastindex >= 4 and vesc.group(4) else None
                try:
                    rate_f = float(rate) if rate and rate != "--" else None
                except ValueError:
                    rate_f = None
                vesc_stats.append((int(vesc.group(1)), float(vesc.group(2)), int(vesc.group(3)), rate_f))

    # --- Report ---
    print("=" * 60)
    print(f"LOG EVALUATOR: {path.name}")
    print("=" * 60)

    if not data_rows:
        print("No data rows parsed. Check log format.")
        sys.exit(0)

    n = len(data_rows)
    rolls = [r["roll"] for r in data_rows]
    errs = [r["err"] for r in data_rows]
    roll_outs = [r["roll_out"] for r in data_rows]
    lefts = [r["left"] for r in data_rows]
    rights = [r["right"] for r in data_rows]

    def mean(x):
        return sum(x) / len(x) if x else 0

    def std(x):
        if len(x) < 2:
            return 0
        m = mean(x)
        return (sum((v - m) ** 2 for v in x) / (len(x) - 1)) ** 0.5

    print(f"\nData rows:     {n}")
    print(f"Setpoints:     {sorted(setpoints_seen)}")
    print(f"\nRoll (°):      mean={mean(rolls):.3f}  std={std(rolls):.3f}  min={min(rolls):.3f}  max={max(rolls):.3f}")
    print(f"Error (°):     mean={mean(errs):.3f}  std={std(errs):.3f}  min={min(errs):.3f}  max={max(errs):.3f}")
    print(f"RollOut (A):   mean={mean(roll_outs):.3f}  std={std(roll_outs):.3f}  min={min(roll_outs):.3f}  max={max(roll_outs):.3f}")
    print(f"Left (A):      mean={mean(lefts):.3f}  std={std(lefts):.3f}  min={min(lefts):.3f}  max={max(lefts):.3f}")
    print(f"Right (A):     mean={mean(rights):.3f}  std={std(rights):.3f}  min={min(rights):.3f}  max={max(rights):.3f}")

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

    if syncs:
        s = syncs[-1]
        print("\nLast SYNC (tuning snapshot):")
        print(f"  Angle:  Kp={s.get('Kp')}  Ki={s.get('Ki')}  Kd={s.get('Kd')}  setpoint={s.get('setpoint')}°")
        print(f"  Vel:    Kp_vel={s.get('Kp_vel')}  Ki_vel={s.get('Ki_vel')}  velSetpoint={s.get('velSetpoint')}")
        print(f"  Motor:  maxCurrent={s.get('maxCurrent')}A  leftMotorSign={s.get('leftMotorSign')}  rightMotorSign={s.get('rightMotorSign')}")
        print(f"  Vel signs: leftVelSign={s.get('leftVelSign')}  rightVelSign={s.get('rightVelSign')}")

    print("\n" + "=" * 60)


if __name__ == "__main__":
    main()
