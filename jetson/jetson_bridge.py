#!/usr/bin/env python3
"""
Jetson Bridge — standalone (no ROS) interface between Jetson and Teensy.

Wraps TeensyComms with:
  • Auto-connect / reconnect logic
  • Safety watchdog: sends vel=0 steer=0 if no command received within
    WATCHDOG_TIMEOUT_S (default 0.5 s)
  • Simple CLI loop for manual testing

Usage (direct):
    python3 jetson_bridge.py [/dev/ttyACM0]

Usage (as library):
    from jetson_bridge import JetsonBridge
    bridge = JetsonBridge()
    bridge.start()                           # auto-finds Teensy
    bridge.set_velocity(0.3, 0.0)           # 0.3 m/s forward
    state = bridge.get_state()              # dict from TeensyComms.get_data()
    bridge.stop()
"""

import sys
import os
import time
import threading

# Allow running from the jetson/ directory without installing
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tuning_code'))
from teensy_comms import TeensyComms

WATCHDOG_TIMEOUT_S = 0.5    # halt if no command for this many seconds
RECONNECT_INTERVAL_S = 3.0  # seconds between reconnect attempts


class JetsonBridge:
    """High-level Jetson ↔ Teensy interface.

    Thread-safe.  All public methods may be called from any thread.
    """

    def __init__(self, device: str = None, watchdog_timeout: float = WATCHDOG_TIMEOUT_S,
                 log_dir: str = None, debug: bool = False):
        """
        Args:
            device:           serial device path; None = auto-detect Teensy
            watchdog_timeout: seconds before velocity is zeroed on no command
            log_dir:          directory for TeensyComms log files
            debug:            enable TeensyComms debug logging
        """
        self.device = device
        self.watchdog_timeout = watchdog_timeout

        self._comms = TeensyComms(
            log_dir=log_dir or os.path.join(os.path.dirname(__file__), 'logs'),
            debug=debug,
            on_disconnect=self._on_disconnect,
        )

        self._lock = threading.Lock()
        self._last_cmd_time = 0.0
        self._running = False
        self._watchdog_thread = None
        self._reconnect_thread = None
        self._disconnected_flag = threading.Event()

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> bool:
        """Connect to Teensy and start watchdog.  Returns True on success."""
        device = self.device or self._auto_detect()
        if not device:
            print("JetsonBridge: no Teensy device found")
            return False

        print(f"JetsonBridge: connecting to {device}")
        if not self._comms.connect(device):
            return False

        self._running = True
        self._last_cmd_time = time.time()  # don't immediately trigger watchdog

        self._watchdog_thread = threading.Thread(
            target=self._watchdog_loop, daemon=True, name="jetson-watchdog"
        )
        self._watchdog_thread.start()

        self._reconnect_thread = threading.Thread(
            target=self._reconnect_loop, daemon=True, name="jetson-reconnect"
        )
        self._reconnect_thread.start()

        print("JetsonBridge: started")
        return True

    def stop(self):
        """Stop watchdog, send a final halt, and disconnect."""
        self._running = False
        if self._comms.connected:
            self._comms.send_jetson_command(0.0, 0.0)
        self._comms.disconnect()
        print("JetsonBridge: stopped")

    # ------------------------------------------------------------------
    # Motion commands
    # ------------------------------------------------------------------

    def set_velocity(self, vel_mps: float, steer_rads: float) -> bool:
        """Send velocity + steering setpoint to the Teensy.

        Args:
            vel_mps:    forward velocity in m/s (negative = reverse)
            steer_rads: yaw rate in rad/s        (positive = turn left)
        Returns True if sent.
        """
        with self._lock:
            self._last_cmd_time = time.time()
        return self._comms.send_jetson_command(vel_mps, steer_rads)

    def halt(self) -> bool:
        """Immediately send zero velocity and steering."""
        return self.set_velocity(0.0, 0.0)

    # ------------------------------------------------------------------
    # State read-back
    # ------------------------------------------------------------------

    def get_state(self) -> dict:
        """Return latest robot state snapshot (thread-safe).

        Keys: imu, comm, tuning, history, connected, last_update,
              roll_sign_inverted, motor_directions_swapped.
        See TeensyComms.get_data() for full schema.
        """
        return self._comms.get_data()

    @property
    def connected(self) -> bool:
        return self._comms.connected

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _auto_detect(self) -> str:
        """Return the first Teensy device found, or None."""
        devices = self._comms.find_devices()
        device_info = getattr(self._comms, 'device_info', {})
        for d in devices:
            if 'Teensy' in device_info.get(d, ''):
                return d
        return devices[0] if devices else None

    def _watchdog_loop(self):
        """Zero velocity if no command received within watchdog_timeout."""
        while self._running:
            time.sleep(0.1)
            if not self._comms.connected:
                continue
            with self._lock:
                age = time.time() - self._last_cmd_time
            if age > self.watchdog_timeout:
                self._comms.send_jetson_command(0.0, 0.0)

    def _on_disconnect(self):
        """Called from TeensyComms reader thread on unexpected link loss."""
        print("JetsonBridge: serial link lost — will attempt reconnect")
        self._disconnected_flag.set()

    def _reconnect_loop(self):
        """Background thread: reconnects after unexpected disconnect."""
        while self._running:
            self._disconnected_flag.wait()
            if not self._running:
                break
            self._disconnected_flag.clear()
            time.sleep(RECONNECT_INTERVAL_S)
            if self._running and not self._comms.connected:
                device = self.device or self._auto_detect()
                if device:
                    print(f"JetsonBridge: reconnecting to {device}")
                    self._comms.connect(device)
                    if self._comms.connected:
                        print("JetsonBridge: reconnected")
                        with self._lock:
                            self._last_cmd_time = time.time()


# ---------------------------------------------------------------------------
# CLI for manual testing
# ---------------------------------------------------------------------------

def _cli(bridge: JetsonBridge):
    print("\nJetson Bridge CLI — commands:")
    print("  f <speed>   forward (m/s), e.g. 'f 0.3'")
    print("  b <speed>   backward (m/s)")
    print("  l <rate>    turn left (rad/s)")
    print("  r <rate>    turn right (rad/s)")
    print("  s           show current state")
    print("  0           halt")
    print("  q           quit\n")

    vel, steer = 0.0, 0.0
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        parts = line.split()
        if not parts:
            continue
        cmd = parts[0].lower()

        if cmd == 'q':
            break
        elif cmd == '0':
            vel, steer = 0.0, 0.0
        elif cmd == 'f' and len(parts) > 1:
            vel   = float(parts[1])
            steer = 0.0
        elif cmd == 'b' and len(parts) > 1:
            vel   = -float(parts[1])
            steer = 0.0
        elif cmd == 'l' and len(parts) > 1:
            steer = float(parts[1])
        elif cmd == 'r' and len(parts) > 1:
            steer = -float(parts[1])
        elif cmd == 's':
            state = bridge.get_state()
            imu = state['imu']
            print(f"  connected:  {state['connected']}")
            print(f"  roll:       {imu['roll']:.2f}°")
            print(f"  pitch:      {imu['pitch']:.2f}°")
            print(f"  yaw:        {imu['yaw']:.2f}°")
            print(f"  vel actual: {imu['velocity_actual']:.3f} m/s")
            print(f"  vel setpt:  {imu['velocity_setpoint']:.3f} m/s")
            continue
        else:
            print(f"Unknown command: {line}")
            continue

        ok = bridge.set_velocity(vel, steer)
        print(f"  -> vel={vel:+.3f} m/s  steer={steer:+.3f} rad/s  {'OK' if ok else 'FAIL'}")


def main():
    device = sys.argv[1] if len(sys.argv) > 1 else None
    bridge = JetsonBridge(device=device, debug=False)

    if not bridge.start():
        print("Failed to start bridge")
        sys.exit(1)

    try:
        _cli(bridge)
    finally:
        bridge.stop()


if __name__ == "__main__":
    main()
