#!/usr/bin/env python3
"""
Teensy serial communications module.

Shared between the tuning GUI (tuning_code/robot_tuning_gui.py) and the
Jetson bridge (jetson/jetson_bridge.py) — single source of truth for the
R: telemetry protocol and # Jetson motion command protocol.

Telemetry rate: 20 Hz (50 ms intervals, driven by Teensy firmware).
Baud rate: 2 Mbaud (DEFAULT_BAUD).

Telemetry formats parsed (all start with 'R:'):
  Phase-1 cascaded  R:roll,P:pitch,Y:yaw,Err:...,Vel:...,VelSet:...,VelPID:...,
                    RollOut:...,YawOut:...,Left:...,Right:...,Setpt:...,
                    Mode:...,Yaw:...,Log:...
  Old cascaded      R:roll,P:pitch,Y:yaw,Pos:...,VelSet:...,VelAct:...,
                    Curr:...,Bal:...,Log:...
  Single-loop       R:roll,P:pitch,Y:yaw,Err:...,YawErr:...,RollOut:...,
                    YawOut:...,Left:...,Right:...,Setpt:...,Drive:...,
                    Mode:...,Yaw:...,Log:...

Jetson motion command sent TO the Teensy:
  #VEL=<m/s>,STEER=<rad/s>    (newline terminated)
  Requires Teensy firmware support — see firmware/teensy_balance_cascaded/.

Optional callbacks (called from the reader thread, not the main thread):
  on_disconnect()          fired when the link goes away unexpectedly
  on_mode_change(str)      fired when the firmware mode token changes
"""

import glob
import os
import re
import threading
import time
from collections import deque
from datetime import datetime

import serial
import serial.tools.list_ports
from serial import SerialException

DEFAULT_BAUD = 2_000_000
MAX_HISTORY = 200  # depth of circular history buffers


class TeensyComms:
    """Thread-safe serial interface to the Teensy balance controller.

    Instantiate, call connect(device), then poll get_data() from any thread.
    send_command(char) sends single-char tuning commands (GUI protocol).
    send_jetson_command(vel, steer) sends J: motion commands (Jetson protocol).
    """

    def __init__(self, log_dir=None, debug=True,
                 on_disconnect=None, on_mode_change=None):
        """
        Args:
            log_dir:        directory for debug + data log files (default "logs")
            debug:          write a timestamped debug log file
            on_disconnect:  callable() invoked from reader thread on link loss
            on_mode_change: callable(mode_str) invoked when firmware mode changes
        """
        self.ser = None
        self.running = False
        self.thread = None
        self.lock = threading.Lock()
        self.log_dir = log_dir or "logs"
        self.debug = debug
        self.debug_log_file = None
        self.on_disconnect = on_disconnect
        self.on_mode_change = on_mode_change
        self._init_debug_log()

        # Latest decoded state (all protected by self.lock)
        self.imu_data = {
            'roll': 0.0,
            'pitch': 0.0,
            'yaw': 0.0,
            'position': 0.0,
            'velocity_setpoint': 0.0,
            'velocity_actual': 0.0,
            'current': 0.0,
            'roll_pid_output': 0.0,
            'yaw_pid_output': 0.0,
            'left_motor_current': 0.0,
            'right_motor_current': 0.0,
            'balance_status': 'UNKNOWN',
            'logging': 'OFF',
        }

        self.comm_stats = {
            'imu_success': 0,
            'imu_fail': 0,
            'imu_rate': 0.0,
            'vesc_success': 0,
            'vesc_fail': 0,
            'vesc_rate': 0.0,
            'imu_hz': 0.0,
            'vesc_hz': 0.0,
        }

        self.tuning_values = {
            # Cascaded PID
            'Kp_angle': 8.0, 'Ki_angle': 0.2, 'Kd_angle': 0.4,
            'Kp_vel': 0.1,   'Ki_vel': 0.0,   'Kd_vel': 0.0,
            'Kp_position': 0.0,
            # Single-loop PID
            'Kp': 5.0, 'Ki': 0.1, 'Kd': 0.3,
            # Yaw PID
            'Kp_yaw': 0.5, 'Ki_yaw': 0.0, 'Kd_yaw': 0.1,
            'yaw_control_enabled': True,
            # Common
            'angle_setpoint': 0.0,
            'max_current': 6.0,
            'min_current': 0.3,
            'deadband': 0.0,
            'velocity_damping': 0.0,
            'drive_offset': 0.0,
            'velocity_setpoint': 0.0,
        }

        self.roll_sign_inverted = False
        self.motor_directions_swapped = True

        # Circular history buffers (useful for GUI plots and offline analysis)
        self.history = {
            'roll':    deque(maxlen=MAX_HISTORY),
            'pitch':   deque(maxlen=MAX_HISTORY),
            'current': deque(maxlen=MAX_HISTORY),
            'time':    deque(maxlen=MAX_HISTORY),
        }

        self.start_time = time.time()
        self.last_data_time = 0
        self.connected = False

        # Optional data log
        self.log_file = None
        self.log_enabled = True
        self.log_filename = None

    # ------------------------------------------------------------------
    # Debug logging
    # ------------------------------------------------------------------

    def _init_debug_log(self):
        if not self.debug:
            return
        try:
            os.makedirs(self.log_dir, exist_ok=True)
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            path = os.path.join(self.log_dir, f"teensy_debug_{ts}.log")
            self.debug_log_file = open(path, 'w', encoding='utf-8')
            self.debug_log_file.write(
                f"# TeensyComms debug log - {datetime.now()}\n"
                "# " + "=" * 70 + "\n\n"
            )
            self.debug_log_file.flush()
            self._debug_log("DEBUG LOG INITIALIZED")
            print(f"Debug log: {path}")
        except Exception as e:
            print(f"Warning: could not create debug log: {e}")
            self.debug_log_file = None

    def _debug_log(self, message, level="INFO"):
        if self.debug_log_file:
            try:
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                self.debug_log_file.write(f"[{ts}] [{level}] {message}\n")
                self.debug_log_file.flush()
            except Exception:
                pass
        if level in ("ERROR", "WARNING"):
            print(f"[{level}] {message}")

    # ------------------------------------------------------------------
    # Device discovery
    # ------------------------------------------------------------------

    def find_devices(self):
        """Return list of serial device paths, Teensy devices first."""
        devices = []
        device_info = {}

        ports = serial.tools.list_ports.comports()
        for port in ports:
            device_path = port.device
            display_parts = []

            is_teensy = port.description and 'Teensy' in port.description
            if is_teensy:
                display_parts.append("Teensy 4.1")
            elif port.description:
                display_parts.append(port.description)

            usb_bus_id = None
            if device_path.startswith('/dev/'):
                dev_name = device_path.replace('/dev/', '')
                for sysfs_path in [
                    f"/sys/class/tty/{dev_name}/device/../../../../uevent",
                    f"/sys/class/tty/{dev_name}/device/../../../uevent",
                ]:
                    if os.path.exists(sysfs_path):
                        try:
                            with open(sysfs_path) as f:
                                for line in f:
                                    if line.startswith('DEVPATH='):
                                        parts = line.split('=')[1].strip().split('/')
                                        for i, part in enumerate(parts):
                                            if part.startswith('usb') and i + 1 < len(parts):
                                                usb_bus_id = f"{part}/{parts[i+1]}"
                                                break
                                        if usb_bus_id:
                                            break
                        except Exception:
                            pass
                        if usb_bus_id:
                            break

            if not usb_bus_id and hasattr(port, 'location') and port.location:
                loc = str(port.location)
                if '/' in loc or '-' in loc:
                    usb_bus_id = f"usb{loc}"

            if usb_bus_id:
                display_parts.append(usb_bus_id)

            display_name = (
                ' '.join(display_parts) if len(display_parts) > 1
                else f"{display_parts[0]} - {device_path}" if display_parts
                else device_path
            )
            devices.append(device_path)
            device_info[device_path] = display_name

        for pattern in ('/dev/ttyACM*', '/dev/ttyUSB*', '/dev/ttyS*'):
            for dev in glob.glob(pattern):
                if dev not in devices:
                    devices.append(dev)
                    device_info[dev] = dev

        self.device_info = device_info

        teensy = [d for d in devices if 'Teensy' in device_info.get(d, '')]
        preferred = [d for d in devices if 'ACM' in d or 'USB' in d or 'ttyS' in d]
        if teensy:
            return teensy + [d for d in preferred if d not in teensy] + \
                   [d for d in devices if d not in preferred and d not in teensy]
        return preferred + [d for d in devices if d not in preferred] if preferred else devices

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self, device):
        """Open the serial port and start the reader thread.

        Returns True on success, False on failure (error printed to stdout).
        """
        self._debug_log(f"Connecting to {device}")
        try:
            # Quick availability check
            try:
                test = serial.Serial(device, DEFAULT_BAUD, timeout=0.1)
                test.close()
            except SerialException as e:
                if "Permission denied" in str(e) or "Access denied" in str(e):
                    raise Exception(
                        f"Port {device} is locked by another program. "
                        "Close Arduino IDE, minicom, etc."
                    )
                raise

            self.ser = serial.Serial(device, DEFAULT_BAUD, timeout=0.1)
            self.connected = True
            self.running = True
            self.start_time = time.time()

            if self.log_enabled:
                self._open_log_file()

            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            self._debug_log("Reader thread started")

            time.sleep(0.3)  # let the port settle before requesting sync
            self.request_sync()
            return True

        except SerialException as e:
            msg = (
                f"Serial connection failed: {e}\n"
                "Check: device connected, correct port, no other program using it."
            )
            self._debug_log(msg, "ERROR")
            print(f"Connection Error: {msg}")
            return False
        except Exception as e:
            import traceback
            self._debug_log(f"{type(e).__name__}: {e}\n{traceback.format_exc()}", "ERROR")
            print(f"Connection Error: {e}")
            return False

    def disconnect(self):
        """Stop reader thread and close the serial port."""
        self._debug_log("Disconnecting")
        self.running = False

        if self.thread:
            self.thread.join(timeout=2.0)
            if self.thread.is_alive():
                self._debug_log("Reader thread did not exit cleanly", "WARNING")

        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception as e:
                self._debug_log(f"Error closing port: {e}", "ERROR")
            finally:
                self.ser = None

        self.connected = False
        self._close_log_file()

        if self.debug_log_file:
            try:
                self.debug_log_file.write(
                    f"\n# Log ended: {datetime.now()}\n"
                )
                self.debug_log_file.close()
            except Exception:
                pass
            self.debug_log_file = None

    # ------------------------------------------------------------------
    # Data log
    # ------------------------------------------------------------------

    def _open_log_file(self):
        if self.log_file is not None:
            return
        os.makedirs(self.log_dir, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_filename = os.path.join(self.log_dir, f"robot_log_{ts}.txt")
        try:
            self.log_file = open(self.log_filename, 'w', encoding='utf-8')
            self.log_file.write(
                f"# Robot serial log — {datetime.now()}\n"
                f"# Device: {self.ser.port if self.ser else 'unknown'}  "
                f"Baud: {DEFAULT_BAUD}\n"
                "# " + "=" * 70 + "\n\n"
            )
            self.log_file.flush()
        except Exception as e:
            print(f"Warning: could not open log file: {e}")
            self.log_file = None

    def _close_log_file(self):
        if self.log_file:
            try:
                self.log_file.write(f"\n# Log ended: {datetime.now()}\n")
                self.log_file.close()
            except Exception:
                pass
            self.log_file = None
            self.log_filename = None

    def toggle_logging(self):
        self.log_enabled = not self.log_enabled
        if self.log_enabled and self.connected and self.log_file is None:
            self._open_log_file()
        elif not self.log_enabled:
            self._close_log_file()
        return self.log_enabled

    def get_log_filename(self):
        return self.log_filename

    # ------------------------------------------------------------------
    # Command sending
    # ------------------------------------------------------------------

    def send_command(self, cmd):
        """Send a single-character tuning command (GUI protocol).

        cmd: single ASCII character, e.g. 'P', 'p', 'k'.
        Returns True if sent successfully.
        """
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(cmd.encode())
                self.ser.flush()
                if self.log_file and self.log_enabled:
                    try:
                        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        self.log_file.write(
                            f"[{ts}] >>> COMMAND: {self._command_name(cmd)} ('{cmd}')\n"
                        )
                        self.log_file.flush()
                    except Exception:
                        pass
                return True
            except Exception:
                return False
        return False

    def send_jetson_command(self, vel_mps: float, steer_rads: float) -> bool:
        """Send a Jetson velocity+steering setpoint to the Teensy.

        Sends: #VEL=<vel_mps>,STEER=<steer_rads>\n
        The Teensy firmware parses '#' lines (see firmware/teensy_balance_cascaded/).

        Args:
            vel_mps:    forward velocity in m/s  (negative = reverse)
            steer_rads: yaw rate in rad/s         (positive = turn left)
        Returns True if sent successfully.
        """
        if self.ser and self.ser.is_open:
            try:
                cmd = f"#VEL={vel_mps:.3f},STEER={steer_rads:.3f}\n"
                self.ser.write(cmd.encode())
                self.ser.flush()
                return True
            except Exception:
                return False
        return False

    def request_sync(self):
        """Ask the firmware to echo all current PID parameters (SYNC: line)."""
        if self.connected and self.ser:
            try:
                self.ser.write(b'@')
                self.ser.flush()
                self._debug_log("Sync requested")
            except Exception as e:
                self._debug_log(f"Sync request failed: {e}", "ERROR")

    @staticmethod
    def _command_name(cmd):
        names = {
            'p': 'Decrease Angle Kp',   'P': 'Increase Angle Kp',
            'i': 'Decrease Angle Ki',   'I': 'Increase Angle Ki',
            'j': 'Decrease Angle Kd',   'D': 'Increase Angle Kd',
            'v': 'Decrease Vel Setpt',  'V': 'Increase Vel Setpt',
            '0': 'Stop (vel=0)',
            'w': 'Decrease Vel Kp',     'W': 'Increase Vel Kp',
            'e': 'Decrease Vel Ki',     'E': 'Increase Vel Ki',
            'm': 'Decrease Max Current','M': 'Increase Max Current',
            'z': 'Decrease Angle Setpt','Z': 'Increase Angle Setpt',
            'k': 'Save Settings',       'K': 'Save Settings',
            'g': 'Load Settings',       'G': 'Load Settings',
            'y': 'Decrease Yaw Kp',     'Y': 'Increase Yaw Kp',
            'u': 'Decrease Yaw Ki',     'U': 'Increase Yaw Ki',
            'h': 'Decrease Yaw Kd',     'H': 'Increase Yaw Kd',
            'n': 'Toggle Yaw Control',  'N': 'Toggle Yaw Control',
            'd': 'Toggle Diagnostic',
            't': 'Toggle Fine Adjust',  'T': 'Toggle Fine Adjust',
            'x': 'Show Tuning Values',  'X': 'Show Tuning Values',
            ' ': 'Toggle Data Stream',
            'l': 'Start Logging',       'L': 'Start Logging',
            's': 'Stop Logging',        'S': 'Stop Logging',
            'b': 'Download Log',        'B': 'Download Log',
            'c': 'Clear Log Buffer',    'C': 'Clear Log Buffer',
            'q': 'Reduce I2C Speed',    'Q': 'Reduce I2C Speed',
        }
        return names.get(cmd, f"Unknown: '{cmd}'")

    # ------------------------------------------------------------------
    # Reader thread
    # ------------------------------------------------------------------

    def _read_loop(self):
        self._debug_log("Reader thread started")
        buf = ""
        error_count = 0
        max_errors = 10
        loop_count = 0
        last_data_time = time.time()

        while self.running:
            loop_count += 1
            try:
                if not self.ser or not self.ser.is_open:
                    self._debug_log("Port closed, exiting reader", "ERROR")
                    if self.running:
                        self.connected = False
                    break

                try:
                    waiting = self.ser.in_waiting
                except Exception as e:
                    error_count += 1
                    self._debug_log(f"in_waiting error #{error_count}: {e}", "ERROR")
                    if error_count >= max_errors:
                        self.connected = False
                        if self.on_disconnect:
                            self.on_disconnect()
                        break
                    time.sleep(0.5)
                    continue

                if waiting > 0:
                    try:
                        raw = self.ser.read(waiting)
                        data = raw.decode('utf-8', errors='ignore')
                        buf += data
                        error_count = 0
                        last_data_time = time.time()

                        if self.log_file and self.log_enabled:
                            try:
                                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                                prefix = "*** MOTOR CMD: " if 'Curr:' in data else ""
                                self.log_file.write(f"[{ts}] {prefix}{data}")
                                self.log_file.flush()
                            except Exception:
                                pass

                        while '\n' in buf:
                            line, buf = buf.split('\n', 1)
                            self._process_line(line.strip())

                    except (SerialException, OSError) as e:
                        error_count += 1
                        self._debug_log(f"Serial error #{error_count}: {e}", "ERROR")
                        if error_count >= max_errors:
                            self.connected = False
                            if self.on_disconnect:
                                self.on_disconnect()
                            break
                        time.sleep(0.5)
                else:
                    if time.time() - last_data_time > 5.0 and loop_count % 500 == 0:
                        self._debug_log(
                            f"No data for {time.time() - last_data_time:.1f}s", "WARNING"
                        )
                    time.sleep(0.01)

            except Exception as e:
                import traceback
                error_count += 1
                self._debug_log(
                    f"Unexpected error #{error_count}: {type(e).__name__}: {e}\n"
                    f"{traceback.format_exc()}", "ERROR"
                )
                if error_count >= max_errors:
                    self.connected = False
                    if self.on_disconnect:
                        self.on_disconnect()
                    break
                time.sleep(0.5)

        self._debug_log("Reader thread exited")

    # ------------------------------------------------------------------
    # Line parser
    # ------------------------------------------------------------------

    def _process_line(self, line):
        tuning_updates = {}
        comm_updates = {}
        imu_update = None
        mode_str = None

        if line.startswith('SYNC:'):
            self._parse_sync_data(line)
            return

        # --- Phase-1 cascaded (velocity control active) ---
        m = re.search(
            r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),'
            r'Err:([-\d.]+),YawErr:([-\d.]+),'
            r'Vel:([-\d.]+),VelSet:([-\d.]+),VelPID:([-\d.]+),'
            r'RollOut:([-\d.]+),YawOut:([-\d.]+),'
            r'Left:([-\d.]+),Right:([-\d.]+),'
            r'Setpt:([-\d.]+),Mode:(\w+),Yaw:(\w+),Log:(\w+)',
            line
        )
        if m:
            imu_update = {
                'roll': float(m.group(1)), 'pitch': float(m.group(2)),
                'yaw': float(m.group(3)), 'position': 0.0,
                'velocity_actual': float(m.group(6)),
                'velocity_setpoint': float(m.group(7)),
                'current': float(m.group(9)),
                'roll_pid_output': float(m.group(9)),
                'yaw_pid_output': float(m.group(10)),
                'left_motor_current': float(m.group(11)),
                'right_motor_current': float(m.group(12)),
                'balance_status': 'OK',
                'logging': m.group(16),
            }
            mode_str = m.group(14)
            tuning_updates['angle_setpoint'] = float(m.group(13))
            tuning_updates['velocity_setpoint'] = float(m.group(7))
            tuning_updates['velocity_pid_output'] = float(m.group(8))
            tuning_updates['yaw_control_enabled'] = (m.group(15) == "ON")

        if not m:
            # --- Old cascaded ---
            m = re.search(
                r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),'
                r'Pos:([-\d.]+),VelSet:([-\d.]+),VelAct:([-\d.]+),'
                r'Curr:([-\d.]+),Bal:(\w+),Log:(\w+)',
                line
            )
            if m:
                imu_update = {
                    'roll': float(m.group(1)), 'pitch': float(m.group(2)),
                    'yaw': float(m.group(3)), 'position': float(m.group(4)),
                    'velocity_setpoint': float(m.group(5)),
                    'velocity_actual': float(m.group(6)),
                    'current': float(m.group(7)),
                    'balance_status': m.group(8), 'logging': m.group(9),
                }
                tuning_updates['velocity_setpoint'] = float(m.group(5))

        if not m:
            # --- Single-loop with motor currents ---
            m = re.search(
                r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),'
                r'Err:([-\d.]+),YawErr:([-\d.]+),'
                r'RollOut:([-\d.]+),YawOut:([^,]+),'
                r'Left:([-\d.]+),Right:([-\d.]+),'
                r'Setpt:([-\d.]+),Drive:([-\d.]+),'
                r'Mode:(\w+),Yaw:(\w+),Log:(\w+)',
                line
            )
            if m:
                try:
                    yaw_out = float(m.group(7)) if 'nan' not in m.group(7).lower() else 0.0
                except (ValueError, TypeError):
                    yaw_out = 0.0
                imu_update = {
                    'roll': float(m.group(1)), 'pitch': float(m.group(2)),
                    'yaw': float(m.group(3)), 'position': 0.0,
                    'velocity_setpoint': 0.0, 'velocity_actual': 0.0,
                    'current': float(m.group(6)),
                    'roll_pid_output': float(m.group(6)),
                    'yaw_pid_output': yaw_out,
                    'left_motor_current': float(m.group(8)),
                    'right_motor_current': float(m.group(9)),
                    'balance_status': 'OK', 'logging': m.group(14),
                }
                mode_str = m.group(12)
                tuning_updates['angle_setpoint'] = float(m.group(10))
                tuning_updates['drive_offset'] = float(m.group(11))
                tuning_updates['yaw_control_enabled'] = (m.group(13) == "ON")

        if not m:
            # --- Fallback: single-loop without yaw fields ---
            m = re.search(
                r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),'
                r'Err:([-\d.]+),Curr:([-\d.]+),'
                r'Setpt:([-\d.]+)(?:,Drive:([-\d.]+))?,'
                r'Mode:(\w+),Log:(\w+)',
                line
            )
            if m:
                imu_update = {
                    'roll': float(m.group(1)), 'pitch': float(m.group(2)),
                    'yaw': float(m.group(3)), 'position': 0.0,
                    'velocity_setpoint': 0.0, 'velocity_actual': 0.0,
                    'current': float(m.group(5)),
                    'balance_status': 'OK', 'logging': m.group(9),
                }
                mode_str = m.group(8)
                tuning_updates['angle_setpoint'] = float(m.group(6))
                if m.group(7) is not None:
                    tuning_updates['drive_offset'] = float(m.group(7))

        # --- Commit IMU snapshot ---
        if imu_update:
            t = time.time() - self.start_time
            with self.lock:
                self.last_data_time = time.time()
                self.imu_data.update(imu_update)
                self.history['time'].append(t)
                self.history['roll'].append(imu_update['roll'])
                self.history['pitch'].append(imu_update['pitch'])
                self.history['current'].append(imu_update['current'])

            if mode_str and self.on_mode_change:
                try:
                    self.on_mode_change(mode_str)
                except Exception:
                    pass

        # --- Comm stats ---
        m = re.search(r'I2C Stats: Success=(\d+)\s+\(([\d.]+)%\),\s+Fail=(\d+)', line)
        if m:
            r = float(m.group(2))
            comm_updates.update({
                'imu_success': int(m.group(1)), 'imu_fail': int(m.group(3)),
                'imu_rate': r, 'imu_hz': 400.0 * r / 100.0,
            })
        m = re.search(r'VESC Stats: Success=(\d+)\s+\(([\d.]+)%\),\s+Fail=(\d+)', line)
        if m:
            r = float(m.group(2))
            comm_updates.update({
                'vesc_success': int(m.group(1)), 'vesc_fail': int(m.group(3)),
                'vesc_rate': r, 'vesc_hz': 67.0 * r / 100.0,
            })

        # --- Tuning value updates ---
        # Ordered most-specific first to prevent substring false-matches.
        for pattern, key in [
            (r'\bVelocity\s+Kp\s*[=:]\s*([\d.]+)',  'Kp_vel'),
            (r'\bVelocity\s+Ki\s*[=:]\s*([\d.]+)',  'Ki_vel'),
            (r'\bVelocity\s+Kd\s*[=:]\s*([\d.]+)',  'Kd_vel'),
            (r'\bVel\s+Kp\s*=\s*([\d.]+)',           'Kp_vel'),
            (r'\bVel\s+Ki\s*=\s*([\d.]+)',           'Ki_vel'),
            (r'\bVel\s+Kd\s*=\s*([\d.]+)',           'Kd_vel'),
            (r'\bKp_vel:\s+([\d.]+)',                 'Kp_vel'),
            (r'\bKi_vel:\s+([\d.]+)',                 'Ki_vel'),
            (r'\bKd_vel:\s+([\d.]+)',                 'Kd_vel'),
            (r'\bRoll Kp\s*[=:]\s*([\d.]+)',         'Kp'),
            (r'\bRoll Ki\s*[=:]\s*([\d.]+)',         'Ki'),
            (r'\bRoll Kd\s*[=:]\s*([\d.]+)',         'Kd'),
            (r'\bAngle Kp\s*=\s*([\d.]+)',           'Kp'),
            (r'\bAngle Ki\s*=\s*([\d.]+)',           'Ki'),
            (r'\bAngle Kd\s*=\s*([\d.]+)',           'Kd'),
            (r'Kp_angle:\s+([\d.]+)',                 'Kp_angle'),
            (r'Ki_angle:\s+([\d.]+)',                 'Ki_angle'),
            (r'Kd_angle:\s+([\d.]+)',                 'Kd_angle'),
            (r'\bYaw Kp\s*[=:]\s*([\d.]+)',          'Kp_yaw'),
            (r'\bYaw Ki\s*[=:]\s*([\d.]+)',          'Ki_yaw'),
            (r'\bYaw Kd\s*[=:]\s*([\d.]+)',          'Kd_yaw'),
            (r'\bKp_yaw[=:]\s*([\d.]+)',             'Kp_yaw'),
            (r'\bKi_yaw[=:]\s*([\d.]+)',             'Ki_yaw'),
            (r'\bKd_yaw[=:]\s*([\d.]+)',             'Kd_yaw'),
            (r'Max Current:\s+([\d.]+)A',             'max_current'),
            (r'Max Current\s*=\s*([\d.]+)A',          'max_current'),
            (r'(?:Base )?Angle Setpoint\s*[=:]\s*([-\d.]+)', 'angle_setpoint'),
            (r'Deadband\s*[=:]\s*([\d.]+)',           'deadband'),
            (r'Drive Offset\s*[=:]\s*([-\d.]+)',      'drive_offset'),
            (r'Yaw Control:\s+(\w+)',                  'yaw_control_enabled'),
        ]:
            match = re.search(pattern, line)
            if match:
                if key == 'yaw_control_enabled':
                    tuning_updates[key] = match.group(1).upper() == "ENABLED"
                else:
                    tuning_updates[key] = float(match.group(1))
                break

        # Combined-gain lines
        m = re.search(r'Roll Gains:\s+Kp=([\d.]+),\s+Ki=([\d.]+),\s+Kd=([\d.]+)', line)
        if m:
            tuning_updates.update({'Kp': float(m.group(1)), 'Ki': float(m.group(2)),
                                   'Kd': float(m.group(3))})

        # Control-direction flags
        m = re.search(r'Motor Directions:\s+(\w+)', line)
        if m:
            with self.lock:
                self.motor_directions_swapped = (m.group(1) == "SWAPPED")
        m = re.search(r'Roll Sign:\s+(\w+)', line)
        if m:
            with self.lock:
                self.roll_sign_inverted = (m.group(1) == "INVERTED")

        if tuning_updates or comm_updates:
            with self.lock:
                self.tuning_values.update(tuning_updates)
                self.comm_stats.update(comm_updates)

    # ------------------------------------------------------------------
    # SYNC handler
    # ------------------------------------------------------------------

    def _parse_sync_data(self, line):
        """Parse SYNC:key=val,... parameter dump from firmware."""
        self._debug_log(f"Parsing SYNC: {line[:80]}")
        key_map = {
            'Kp': 'Kp', 'Ki': 'Ki', 'Kd': 'Kd',
            'Kp_vel': 'Kp_vel', 'Ki_vel': 'Ki_vel', 'Kd_vel': 'Kd_vel',
            'Kp_yaw': 'Kp_yaw', 'Ki_yaw': 'Ki_yaw', 'Kd_yaw': 'Kd_yaw',
            'setpoint': 'angle_setpoint', 'maxCurrent': 'max_current',
            'velSetpoint': 'velocity_setpoint',
            'yawEnabled': 'yaw_control_enabled',
            'fineAdjust': 'fine_adjust',
        }
        updates = {}
        for pair in line[5:].split(','):
            if '=' not in pair:
                continue
            k, v = pair.split('=', 1)
            gui_key = key_map.get(k.strip())
            if gui_key:
                try:
                    if gui_key in ('yaw_control_enabled', 'fine_adjust'):
                        updates[gui_key] = (v.strip() == '1')
                    else:
                        updates[gui_key] = float(v.strip())
                except ValueError:
                    self._debug_log(f"Bad SYNC value: {k}={v}", "WARNING")
        if updates:
            with self.lock:
                self.tuning_values.update(updates)
            self._debug_log(f"SYNC: updated {len(updates)} params")
            print(f"Synced {len(updates)} params from firmware")

    # ------------------------------------------------------------------
    # Thread-safe state accessor
    # ------------------------------------------------------------------

    def get_data(self):
        """Return a snapshot of all current state (thread-safe).

        Returns a dict with keys:
            imu       – latest R: telemetry values
            comm      – I2C / VESC communication statistics
            tuning    – PID and control parameters
            history   – dict of lists (roll, pitch, current, time)
            connected – bool
            last_update – timestamp of last R: line received
            roll_sign_inverted      – bool
            motor_directions_swapped – bool
        """
        with self.lock:
            return {
                'imu':     self.imu_data.copy(),
                'comm':    self.comm_stats.copy(),
                'tuning':  self.tuning_values.copy(),
                'history': {k: list(v) for k, v in self.history.items()},
                'connected': self.connected,
                'last_update': self.last_data_time,
                'roll_sign_inverted': self.roll_sign_inverted,
                'motor_directions_swapped': self.motor_directions_swapped,
            }
