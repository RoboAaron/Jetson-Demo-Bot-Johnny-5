#!/usr/bin/env python3
"""
Robot Tuning GUI - Real-time monitoring and parameter adjustment
for self-balancing robot
"""

import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
from serial import SerialException
import threading
import re
import time
from collections import deque
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import glob
from datetime import datetime
import os

# Constants
DEFAULT_BAUD = 2000000
MAX_PLOT_POINTS = 200
UPDATE_INTERVAL_MS = 50  # GUI update rate (20 Hz)

class SerialReader:
    """Thread-safe serial communication handler"""
    def __init__(self, log_dir=None, debug=True):
        self.ser = None
        self.running = False
        self.thread = None
        self.lock = threading.Lock()
        self.log_dir = log_dir or "logs"  # Default to "logs" in current directory
        self.debug = debug
        self.debug_log_file = None
        self._init_debug_log()
        
        # Data storage
        self.imu_data = {
            'roll': 0.0,
            'pitch': 0.0,
            'yaw': 0.0,
            'position': 0.0,
            'velocity_setpoint': 0.0,
            'velocity_actual': 0.0,
            'current': 0.0,
            'roll_pid_output': 0.0,  # Roll PID output (before yaw correction)
            'yaw_pid_output': 0.0,  # Yaw PID output
            'left_motor_current': 0.0,  # Final left motor current (after yaw correction)
            'right_motor_current': 0.0,  # Final right motor current (after yaw correction)
            'balance_status': 'UNKNOWN',
            'logging': 'OFF'
        }
        
        self.comm_stats = {
            'imu_success': 0,
            'imu_fail': 0,
            'imu_rate': 0.0,
            'vesc_success': 0,
            'vesc_fail': 0,
            'vesc_rate': 0.0,
            'imu_hz': 0.0,
            'vesc_hz': 0.0
        }
        
        self.tuning_values = {
            # Cascaded PID (for original firmware)
            'Kp_angle': 8.0,
            'Ki_angle': 0.2,
            'Kd_angle': 0.4,
            'Kp_vel': 0.1,  # Start conservative for Phase 1
            'Ki_vel': 0.0,
            'Kd_vel': 0.0,
            'Kp_position': 0.0,
            # Single-loop PID (for clean firmware)
            'Kp': 5.0,
            'Ki': 0.1,
            'Kd': 0.3,
            'angle_filter_alpha': 0.3,
            # Yaw PID (rotation control)
            'Kp_yaw': 0.5,
            'Ki_yaw': 0.0,
            'Kd_yaw': 0.1,
            'yaw_control_enabled': True,
            # Common parameters
            'angle_setpoint': 0.0,
            'max_current': 6.0,
            'min_current': 0.3,
            'deadband': 0.0,
            'velocity_damping': 0.0,
            'drive_offset': 0.0,
            # Velocity control (Phase 1)
            'velocity_setpoint': 0.0,  # Target velocity in m/s
            'use_vel_loop': False,   # Velocity loop ON/OFF (cascaded mode)
            'motor_output_enabled': True  # Dry-run motor output state (o toggle)
        }
        
        # Control direction state
        self.roll_sign_inverted = False
        self.motor_directions_swapped = True
        
        # Plot data (circular buffers)
        self.plot_data = {
            'roll': deque(maxlen=MAX_PLOT_POINTS),
            'pitch': deque(maxlen=MAX_PLOT_POINTS),
            'current': deque(maxlen=MAX_PLOT_POINTS),
            'time': deque(maxlen=MAX_PLOT_POINTS)
        }
        
        self.start_time = time.time()
        self.last_data_time = 0
        self.connected = False
        
        # Logging
        self.log_file = None
        self.log_enabled = True  # Enable logging by default
        self.log_filename = None
    
    def _init_debug_log(self):
        """Initialize debug log file"""
        if not self.debug:
            return
        
        try:
            # Create logs directory if it doesn't exist
            if not os.path.exists(self.log_dir):
                os.makedirs(self.log_dir)
            
            # Generate timestamped debug log filename
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            debug_log_path = os.path.join(self.log_dir, f"gui_debug_{timestamp}.log")
            
            self.debug_log_file = open(debug_log_path, 'w', encoding='utf-8')
            self.debug_log_file.write(f"# GUI Debug Log - Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            self.debug_log_file.write("# This log tracks all serial operations, errors, and state changes\n")
            self.debug_log_file.write("# " + "="*70 + "\n\n")
            self.debug_log_file.flush()
            self._debug_log("DEBUG LOG INITIALIZED")
            print(f"Debug log created: {debug_log_path}")
        except Exception as e:
            print(f"Warning: Could not create debug log: {e}")
            self.debug_log_file = None
    
    def _debug_log(self, message, level="INFO"):
        """Write to debug log file"""
        if self.debug_log_file:
            try:
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                self.debug_log_file.write(f"[{timestamp}] [{level}] {message}\n")
                self.debug_log_file.flush()
            except:
                pass
        # Also print to console for immediate feedback
        if level in ["ERROR", "WARNING"]:
            print(f"[{level}] {message}")
        
    def find_devices(self):
        """Find available serial devices with USB bus identifiers"""
        devices = []
        device_info = {}  # Store device info: {device_path: display_name}
        
        # Use pyserial's list_ports to get detailed info
        ports = serial.tools.list_ports.comports()
        for port in ports:
            device_path = port.device
            
            # Build display name with USB bus info if available
            display_parts = []
            
            # Check if it's a Teensy
            is_teensy = False
            if port.description and 'Teensy' in port.description:
                display_parts.append("Teensy 4.1")
                is_teensy = True
            elif port.description:
                display_parts.append(port.description)
            
            # Try to get USB bus identifier (e.g., "usb3/3-1")
            usb_bus_id = None
            
            # Method 1: Try to get from sysfs (Linux)
            if device_path.startswith('/dev/'):
                dev_name = device_path.replace('/dev/', '')
                # Try to find USB device path
                sysfs_paths = [
                    f"/sys/class/tty/{dev_name}/device/../../../../uevent",
                    f"/sys/class/tty/{dev_name}/device/../../../uevent",
                ]
                for sysfs_path in sysfs_paths:
                    if os.path.exists(sysfs_path):
                        try:
                            with open(sysfs_path, 'r') as f:
                                for line in f:
                                    if line.startswith('DEVPATH='):
                                        # Extract USB bus path like "usb3/3-1"
                                        devpath = line.split('=')[1].strip()
                                        # Format: /devices/pci0000:00/0000:00:14.0/usb3/3-1/...
                                        parts = devpath.split('/')
                                        for i, part in enumerate(parts):
                                            if part.startswith('usb') and i + 1 < len(parts):
                                                usb_bus_id = f"{part}/{parts[i+1]}"
                                                break
                                        if usb_bus_id:
                                            break
                        except:
                            pass
                        if usb_bus_id:
                            break
            
            # Method 2: Try pyserial's location attribute
            if not usb_bus_id and hasattr(port, 'location') and port.location:
                # Location might be in format like "1-1.4" or similar
                # Try to convert to "usb3/3-1" format
                location = str(port.location)
                if location:
                    # Simple heuristic: if it looks like a USB path, use it
                    if '/' in location or '-' in location:
                        usb_bus_id = f"usb{location}"
            
            # Add USB bus identifier to display
            if usb_bus_id:
                display_parts.append(usb_bus_id)
            
            # Build final display name
            if display_parts:
                # Format: "Teensy 4.1 usb3/3-1" or "Teensy 4.1 usb3/3-1 - /dev/ttyS4"
                if len(display_parts) > 1:
                    display_name = f"{' '.join(display_parts)}"
                else:
                    display_name = f"{display_parts[0]} - {device_path}"
            else:
                display_name = device_path
            
            devices.append(device_path)
            device_info[device_path] = display_name
        
        # Also check common paths that might not be in list_ports
        for pattern in ['/dev/ttyACM*', '/dev/ttyUSB*', '/dev/ttyS*']:
            for device_path in glob.glob(pattern):
                if device_path not in devices:
                    devices.append(device_path)
                    device_info[device_path] = device_path
        
        # Store device info for display
        self.device_info = device_info
        
        # Prefer ACM/USB over ttyS, and prefer Teensy devices
        teensy_devices = [d for d in devices if 'Teensy' in device_info.get(d, '')]
        preferred = [d for d in devices if 'ACM' in d or 'USB' in d or 'ttyS' in d]
        if teensy_devices:
            return teensy_devices + [d for d in preferred if d not in teensy_devices] + [d for d in devices if d not in preferred and d not in teensy_devices]
        elif preferred:
            return preferred + [d for d in devices if d not in preferred]
        return devices
    
    def connect(self, device):
        """Connect to serial device"""
        self._debug_log(f"Attempting to connect to {device}")
        try:
            # Check if port is already in use
            try:
                self._debug_log(f"Testing port availability: {device}")
                test_ser = serial.Serial(device, DEFAULT_BAUD, timeout=0.1)
                test_ser.close()
                self._debug_log("Port test successful - port is available")
            except SerialException as e:
                self._debug_log(f"Port test failed: {e}", "ERROR")
                if "Permission denied" in str(e) or "Access denied" in str(e):
                    raise Exception(f"Port {device} is locked by another program.\n\n"
                                  "Close other programs using this port (Arduino IDE, minicom, etc.)")
                raise
            
            self._debug_log(f"Opening serial connection: {device} @ {DEFAULT_BAUD} baud")
            self.ser = serial.Serial(device, DEFAULT_BAUD, timeout=0.1)
            self._debug_log(f"Serial port opened successfully. Port: {self.ser.port}, Baud: {self.ser.baudrate}")
            
            self.connected = True
            self.running = True
            self.start_time = time.time()
            self._debug_log("Connection state set: connected=True, running=True")
            
            # Open log file if logging is enabled
            if self.log_enabled:
                self._open_log_file()
            
            self._debug_log("Starting read thread")
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            self._debug_log("Read thread started successfully")
            
            # Request parameter sync from firmware after connection
            # Small delay to let read thread start and serial stabilize
            time.sleep(0.3)
            self.request_sync()
            
            return True
        except SerialException as e:
            self._debug_log(f"SerialException during connect: {e}", "ERROR")
            error_msg = f"Serial connection failed:\n{str(e)}\n\n"
            if "Permission denied" in str(e) or "Access denied" in str(e):
                error_msg += "Port is locked by another program.\nClose Arduino IDE, minicom, or other serial programs."
            else:
                error_msg += "Check:\n• Device is connected\n• Correct port selected\n• No other programs using port"
            if hasattr(self, 'gui'):
                messagebox.showerror("Connection Error", error_msg)
            else:
                print(f"Connection Error: {error_msg}")
            return False
        except Exception as e:
            self._debug_log(f"Exception during connect: {type(e).__name__}: {e}", "ERROR")
            import traceback
            self._debug_log(f"Traceback: {traceback.format_exc()}", "ERROR")
            error_msg = f"Failed to connect:\n{str(e)}"
            if hasattr(self, 'gui'):
                messagebox.showerror("Connection Error", error_msg)
            else:
                print(f"Connection Error: {error_msg}")
            return False
    
    def _open_log_file(self):
        """Open a timestamped log file"""
        if self.log_file is not None:
            return  # Already open
        
        # Create logs directory if it doesn't exist
        if not os.path.exists(self.log_dir):
            os.makedirs(self.log_dir)
        
        # Generate timestamped filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_filename = os.path.join(self.log_dir, f"robot_log_{timestamp}.txt")
        
        try:
            self.log_file = open(self.log_filename, 'w', encoding='utf-8')
            # Write header
            self.log_file.write(f"# Robot Serial Log - Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            self.log_file.write(f"# Device: {self.ser.port if self.ser else 'Unknown'}\n")
            self.log_file.write(f"# Baud Rate: {DEFAULT_BAUD}\n")
            self.log_file.write("#\n")
            self.log_file.write("# Data Format:\n")
            self.log_file.write("#   R:Roll(°), P:Pitch(°), Y:Yaw(°), Err:RollError(°), YawErr:YawError(°),\n")
            self.log_file.write("#   RollOut:RollPID_Output(A), YawOut:YawPID_Output(A), Left:LeftMotor(A), Right:RightMotor(A),\n")
            self.log_file.write("#   Setpt:Setpoint(°), Drive:DriveOffset(°), Mode:ControlMode, Yaw:YawControl, Log:LoggingStatus\n")
            self.log_file.write("#\n")
            self.log_file.write("# Motor Commands:\n")
            self.log_file.write("#   Curr value is the motor current command sent to VESCs (important for PID tuning!)\n")
            self.log_file.write("#   Lines with '*** MOTOR CMD:' highlight motor command data\n")
            self.log_file.write("#   Lines with '>>> COMMAND:' show commands sent from GUI to robot\n")
            self.log_file.write("#\n")
            self.log_file.write("# " + "="*70 + "\n\n")
            self.log_file.flush()
        except Exception as e:
            print(f"Warning: Could not open log file: {e}")
            self.log_file = None
    
    def _close_log_file(self):
        """Close the log file"""
        if self.log_file:
            try:
                self.log_file.write(f"\n# Log ended: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                self.log_file.close()
            except:
                pass
            self.log_file = None
            self.log_filename = None
    
    def disconnect(self):
        """Disconnect from serial device"""
        self._debug_log("Disconnect called")
        self.running = False
        self._debug_log("Set running=False")
        
        # Wait for thread to finish (with timeout)
        if self.thread:
            self._debug_log("Waiting for read thread to finish...")
            self.thread.join(timeout=2.0)
            if self.thread.is_alive():
                self._debug_log("WARNING: Read thread did not terminate cleanly", "WARNING")
            else:
                self._debug_log("Read thread terminated successfully")
        
        # Close serial port
        if self.ser:
            try:
                if self.ser.is_open:
                    self._debug_log(f"Closing serial port: {self.ser.port}")
                    self.ser.close()
                    self._debug_log("Serial port closed")
                else:
                    self._debug_log("Serial port already closed")
            except Exception as e:
                self._debug_log(f"Error closing serial port: {e}", "ERROR")
            finally:
                self.ser = None
                self._debug_log("Serial object set to None")
        
        self.connected = False
        self._debug_log("Set connected=False")
        
        # Close log file
        self._close_log_file()
        
        # Close debug log
        if self.debug_log_file:
            try:
                self._debug_log("Closing debug log")
                self.debug_log_file.write(f"\n# Debug log ended: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                self.debug_log_file.close()
            except:
                pass
            self.debug_log_file = None
    
    def toggle_logging(self):
        """Toggle logging on/off"""
        self.log_enabled = not self.log_enabled
        if self.log_enabled and self.connected and self.log_file is None:
            self._open_log_file()
        elif not self.log_enabled:
            self._close_log_file()
        return self.log_enabled
    
    def get_log_filename(self):
        """Get current log filename"""
        return self.log_filename
    
    def send_command(self, cmd):
        """Send command to robot and log it"""
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(cmd.encode())
                self.ser.flush()
                
                # Log command sent to robot
                if self.log_file and self.log_enabled:
                    try:
                        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        cmd_name = self._get_command_name(cmd)
                        self.log_file.write(f"[{timestamp}] >>> COMMAND: {cmd_name} (char: '{cmd}')\n")
                        self.log_file.flush()
                    except:
                        pass
                
                return True
            except:
                return False
        return False
    
    def _get_command_name(self, cmd):
        """Get human-readable name for command - matches firmware exactly"""
        cmd_names = {
            # Angle PID Tuning
            'p': 'Decrease Angle Kp', 'P': 'Increase Angle Kp',
            'i': 'Decrease Angle Ki', 'I': 'Increase Angle Ki',
            'j': 'Decrease Angle Kd', 'D': 'Increase Angle Kd',
            # Velocity Control (Phase 1 Cascaded)
            'v': 'Toggle Velocity Loop ON/OFF',
            '6': 'Decrease Velocity Setpoint',
            'V': 'Increase Velocity Setpoint',
            '0': 'Stop Velocity (set to 0.0)',
            # Velocity PID Tuning (PI only - Kd always 0)
            'w': 'Decrease Velocity Kp', 'W': 'Increase Velocity Kp',
            'e': 'Decrease Velocity Ki', 'E': 'Increase Velocity Ki',
            'r': 'Velocity Kd (disabled - PI only)', 'R': 'Velocity Kd (disabled - PI only)',
            # Motor settings
            'm': 'Decrease Max Current', 'M': 'Increase Max Current',
            'z': 'Decrease Angle Setpoint', 'Z': 'Increase Angle Setpoint',
            # Save/Load Settings
            'k': 'Save Settings', 'K': 'Save Settings',
            'g': 'Load Settings', 'G': 'Load Settings',
            # Yaw PID Tuning
            'y': 'Decrease Yaw Kp', 'Y': 'Increase Yaw Kp',
            'u': 'Decrease Yaw Ki', 'U': 'Increase Yaw Ki',
            'h': 'Decrease Yaw Kd', 'H': 'Increase Yaw Kd',
            'n': 'Toggle Yaw Control', 'N': 'Toggle Yaw Control',
            # Other controls
            'd': 'Toggle Diagnostic Mode',
            't': 'Toggle Fine Adjust', 'T': 'Toggle Fine Adjust',
            'x': 'Show All Tuning Values', 'X': 'Show All Tuning Values',
            ' ': 'Toggle Data Stream',
            'l': 'Start Logging', 'L': 'Start Logging',
            's': 'Stop Logging', 'S': 'Stop Logging',
            'b': 'Download Log Data', 'B': 'Download Log Data',  # When logging enabled (changed from w/W to avoid conflict)
            'c': 'Clear Log Buffer', 'C': 'Clear Log Buffer',
            # Min current (deadzone) control
            'q': 'Decrease Min Current', 'Q': 'Increase Min Current',
        }
        return cmd_names.get(cmd, f"Unknown command: '{cmd}'")
    
    def _read_loop(self):
        """Main serial reading loop (runs in separate thread)"""
        self._debug_log("Read loop started")
        buffer = ""
        error_count = 0
        max_errors = 10  # Disconnect after too many consecutive errors
        loop_count = 0
        last_data_time = time.time()
        
        while self.running:
            loop_count += 1
            try:
                # Check if serial port is still valid
                if not self.ser:
                    self._debug_log("Serial object is None, exiting read loop", "ERROR")
                    if self.running:
                        self.connected = False
                    break
                
                if not self.ser.is_open:
                    self._debug_log("Serial port is not open, exiting read loop", "ERROR")
                    if self.running:
                        self.connected = False
                    break
                
                # Check if data is available (non-blocking)
                try:
                    bytes_waiting = self.ser.in_waiting
                except Exception as e:
                    self._debug_log(f"Error checking in_waiting: {e}", "ERROR")
                    error_count += 1
                    if error_count >= max_errors:
                        self._debug_log(f"Too many errors checking in_waiting ({error_count}), disconnecting", "ERROR")
                        if self.running:
                            self.connected = False
                        break
                    time.sleep(0.5)
                    continue
                
                if bytes_waiting > 0:
                    try:
                        bytes_to_read = bytes_waiting
                        self._debug_log(f"Reading {bytes_to_read} bytes from serial port")
                        raw_data = self.ser.read(bytes_to_read)
                        data = raw_data.decode('utf-8', errors='ignore')
                        buffer += data
                        error_count = 0  # Reset error count on successful read
                        last_data_time = time.time()
                        
                        if loop_count % 100 == 0:  # Log every 100th successful read
                            self._debug_log(f"Read loop healthy: {len(data)} chars, buffer={len(buffer)} chars")
                        
                        # Write raw data to log file if enabled
                        if self.log_file and self.log_enabled:
                            try:
                                # Write with timestamp
                                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # Include milliseconds
                                
                                # Highlight motor command data for easier analysis
                                if 'Curr:' in data:
                                    # This is motor current data - make it more visible
                                    self.log_file.write(f"[{timestamp}] *** MOTOR CMD: {data}")
                                else:
                                    self.log_file.write(f"[{timestamp}] {data}")
                                self.log_file.flush()  # Flush immediately to prevent data loss
                            except Exception as e:
                                print(f"Log write error: {e}")
                        
                        # Process complete lines
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            self._process_line(line.strip())
                    except SerialException as e:
                        # Serial port error - device disconnected or locked
                        error_count += 1
                        self._debug_log(f"SerialException #{error_count}: {e}", "ERROR")
                        if error_count >= max_errors:
                            self._debug_log(f"Too many serial errors ({error_count}), disconnecting...", "ERROR")
                            if self.running:
                                self.connected = False
                                # Notify GUI if available
                                if hasattr(self, 'gui'):
                                    self.gui.root.after(0, self._notify_disconnect)
                            break
                        if self.running and error_count % 5 == 0:  # Log every 5th error
                            self._debug_log(f"Serial error ({error_count}/{max_errors}): {e}", "WARNING")
                        time.sleep(0.5)  # Longer delay after serial errors
                    except OSError as e:
                        # I/O error - device disconnected
                        error_count += 1
                        self._debug_log(f"OSError #{error_count}: {e}", "ERROR")
                        if error_count >= max_errors:
                            self._debug_log(f"Device disconnected ({error_count} errors), disconnecting...", "ERROR")
                            if self.running:
                                self.connected = False
                                # Notify GUI if available
                                if hasattr(self, 'gui'):
                                    self.gui.root.after(0, self._notify_disconnect)
                            break
                        if self.running and error_count % 5 == 0:
                            self._debug_log(f"I/O error ({error_count}/{max_errors}): {e}", "WARNING")
                        time.sleep(0.5)
                else:
                    # No data available, small delay
                    # Check if we've been waiting too long (might indicate connection issue)
                    time_since_data = time.time() - last_data_time
                    if time_since_data > 5.0 and loop_count % 500 == 0:  # Log every 500 loops if no data for 5s
                        self._debug_log(f"No data received for {time_since_data:.1f} seconds", "WARNING")
                    time.sleep(0.01)
                    
            except Exception as e:
                error_count += 1
                self._debug_log(f"Unexpected exception #{error_count}: {type(e).__name__}: {e}", "ERROR")
                import traceback
                self._debug_log(f"Traceback: {traceback.format_exc()}", "ERROR")
                if error_count >= max_errors:
                    self._debug_log(f"Too many unexpected errors ({error_count}), disconnecting", "ERROR")
                    if self.running:
                        self.connected = False
                        # Notify GUI if available
                        if hasattr(self, 'gui'):
                            self.gui.root.after(0, self._notify_disconnect)
                    break
                if self.running and error_count % 5 == 0:
                    self._debug_log(f"Error ({error_count}/{max_errors}): {e}", "WARNING")
                time.sleep(0.5)  # Delay after errors to prevent spam
        
        self._debug_log("Read loop exited")
    
    def _process_line(self, line):
        """Process a line of serial data"""
        # Initialize dictionaries for updates (do this first!)
        tuning_updates = {}
        comm_updates = {}
        imu_update = None
        mode_str = None
        
        # Check for SYNC line (firmware parameter sync on connect)
        # Format: SYNC:Kp=1.50,Ki=0.00,Kd=0.03,Kp_vel=1.00,...
        if line.startswith('SYNC:'):
            self._parse_sync_data(line)
            return  # SYNC line is fully processed, no need for further parsing
        
        # Do regex parsing OUTSIDE the lock to minimize lock time
        # Parse IMU data - support multiple formats:
        # Phase 1 Cascaded: R:roll,P:pitch,Y:yaw,Err:rollError,YawErr:yawError,Vel:velocity,VelSet:velocitySetpoint,VelPID:velocityPID,RollOut:rollPID,YawOut:yawPID,Left:leftMotor,Right:rightMotor,Setpt:setpoint,Mode:mode,Yaw:yawCtrl,Log:logging
        # Old cascaded: R:0.00,P:0.00,Y:0.00,Pos:0.00,VelSet:0.00,VelAct:0.00,Curr:0.00,Bal:OK,Log:OFF
        # New single-loop: R:0.00,P:0.00,Y:0.00,Err:0.00,Curr:0.00,Setpt:0.00,Mode:PID,Log:OFF
        
        # Try Phase 1 cascaded format first (with velocity data)
        # New format includes RawVel: R:...,Vel:filtered,RawVel:raw,VelSet:...,VelPID:...
        match = re.search(r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),Err:([-\d.]+),YawErr:([-\d.]+),Vel:([-\d.]+),RawVel:([-\d.]+),VelSet:([-\d.]+),VelPID:([-\d.]+),RollOut:([-\d.]+),YawOut:([-\d.]+),Left:([-\d.]+),Right:([-\d.]+),Setpt:([-\d.]+),Mode:(\w+),Yaw:(\w+),Log:(\w+)', line)
        if match:
            # New cascaded format (with RawVel)
            imu_update = {
                'roll': float(match.group(1)),
                'pitch': float(match.group(2)),
                'yaw': float(match.group(3)),
                'position': 0.0,
                'velocity_setpoint': float(match.group(8)),
                'velocity_actual': float(match.group(6)),
                'velocity_raw': float(match.group(7)),
                'current': float(match.group(10)),
                'roll_pid_output': float(match.group(10)),
                'yaw_pid_output': float(match.group(11)),
                'left_motor_current': float(match.group(12)),
                'right_motor_current': float(match.group(13)),
                'balance_status': 'OK',
                'logging': match.group(17),
                'control_mode': match.group(15)
            }
            mode_str = match.group(15)
            tuning_updates['angle_setpoint'] = float(match.group(14))
            tuning_updates['velocity_setpoint'] = float(match.group(8))
            tuning_updates['velocity_pid_output'] = float(match.group(9))
            tuning_updates['yaw_control_enabled'] = (match.group(16) == "ON")
        else:
            # Old cascaded format (no RawVel)
            match = re.search(r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),Err:([-\d.]+),YawErr:([-\d.]+),Vel:([-\d.]+),VelSet:([-\d.]+),VelPID:([-\d.]+),RollOut:([-\d.]+),YawOut:([-\d.]+),Left:([-\d.]+),Right:([-\d.]+),Setpt:([-\d.]+),Mode:(\w+),Yaw:(\w+),Log:(\w+)', line)
            if match:
                imu_update = {
                    'roll': float(match.group(1)),
                    'pitch': float(match.group(2)),
                    'yaw': float(match.group(3)),
                    'position': 0.0,
                    'velocity_setpoint': float(match.group(7)),
                    'velocity_actual': float(match.group(6)),
                    'current': float(match.group(9)),
                    'roll_pid_output': float(match.group(9)),
                    'yaw_pid_output': float(match.group(10)),
                    'left_motor_current': float(match.group(11)),
                    'right_motor_current': float(match.group(12)),
                    'balance_status': 'OK',
                    'logging': match.group(16),
                    'control_mode': match.group(14)
                }
                mode_str = match.group(14)
                tuning_updates['angle_setpoint'] = float(match.group(13))
                tuning_updates['velocity_setpoint'] = float(match.group(7))
                tuning_updates['velocity_pid_output'] = float(match.group(8))
                tuning_updates['yaw_control_enabled'] = (match.group(15) == "ON")
            else:
                # Try old cascaded format (Pos, VelSet, VelAct)
                match = re.search(r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),Pos:([-\d.]+),VelSet:([-\d.]+),VelAct:([-\d.]+),Curr:([-\d.]+),Bal:(\w+),Log:(\w+)', line)
                if match:
                    imu_update = {
                        'roll': float(match.group(1)),
                        'pitch': float(match.group(2)),
                        'yaw': float(match.group(3)),
                        'position': float(match.group(4)),
                        'velocity_setpoint': float(match.group(5)),
                        'velocity_actual': float(match.group(6)),
                        'current': float(match.group(7)),
                        'balance_status': match.group(8),
                        'logging': match.group(9)
                    }
                    tuning_updates['velocity_setpoint'] = float(match.group(5))
                else:
                    # Try new single-loop format with motor currents
                    match = re.search(r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),Err:([-\d.]+),YawErr:([-\d.]+),RollOut:([-\d.]+),YawOut:([^,]+),Left:([-\d.]+),Right:([-\d.]+),Setpt:([-\d.]+),Drive:([-\d.]+),Mode:(\w+),Yaw:(\w+),Log:(\w+)', line)
                    if match:
                        yaw_out_str = match.group(7)
                        try:
                            yaw_out = float(yaw_out_str) if 'nan' not in yaw_out_str.lower() else 0.0
                        except (ValueError, TypeError):
                            yaw_out = 0.0
                        imu_update = {
                            'roll': float(match.group(1)),
                            'pitch': float(match.group(2)),
                            'yaw': float(match.group(3)),
                            'position': 0.0,
                            'velocity_setpoint': 0.0,
                            'velocity_actual': 0.0,
                            'current': float(match.group(6)),
                            'roll_pid_output': float(match.group(6)),
                            'yaw_pid_output': yaw_out,
                            'left_motor_current': float(match.group(8)),
                            'right_motor_current': float(match.group(9)),
                            'balance_status': 'OK',
                            'logging': match.group(14),
                            'control_mode': match.group(12)
                        }
                        mode_str = match.group(12)
                        setpoint_from_stream = float(match.group(10))
                        tuning_updates['angle_setpoint'] = setpoint_from_stream
                        tuning_updates['drive_offset'] = float(match.group(11))
                        tuning_updates['yaw_control_enabled'] = (match.group(13) == "ON")
                    else:
                        # Fallback: format without yaw info
                        match = re.search(r'R:([-\d.]+),P:([-\d.]+),Y:([-\d.]+),Err:([-\d.]+),Curr:([-\d.]+),Setpt:([-\d.]+)(?:,Drive:([-\d.]+))?,Mode:(\w+),Log:(\w+)', line)
                        if match:
                            imu_update = {
                                'roll': float(match.group(1)),
                                'pitch': float(match.group(2)),
                                'yaw': float(match.group(3)),
                                'position': 0.0,
                                'velocity_setpoint': 0.0,
                                'velocity_actual': 0.0,
                                'current': float(match.group(5)),
                                'balance_status': 'OK',
                                'logging': match.group(9),
                                'control_mode': match.group(8)
                            }
                            mode_str = match.group(8)
                            setpoint_from_stream = float(match.group(6))
                            tuning_updates['angle_setpoint'] = setpoint_from_stream
                            if match.group(7) is not None:
                                tuning_updates['drive_offset'] = float(match.group(7))
        
        # Now update data structures with minimal lock time
        if imu_update:
            t = time.time() - self.start_time
            with self.lock:
                self.last_data_time = time.time()
                self.imu_data.update(imu_update)
                # Add to plot data
                self.plot_data['time'].append(t)
                self.plot_data['roll'].append(imu_update['roll'])
                self.plot_data['pitch'].append(imu_update['pitch'])
                self.plot_data['current'].append(imu_update['current'])
            
            # Update GUI mode label OUTSIDE the lock (GUI operations are thread-safe)
            if mode_str and hasattr(self, 'gui') and hasattr(self.gui, 'mode_label'):
                try:
                    self.gui.root.after(0, lambda m=mode_str: self.gui.mode_label.config(text=m))
                except:
                    pass
            
        # Parse data streaming status messages (no lock needed)
        if 'Data streaming RESUMED' in line or '✅ Data streaming RESUMED' in line:
            print("INFO: Data streaming is now ON")
        elif 'Data streaming PAUSED' in line or '⏸️  Data streaming PAUSED' in line:
            print("WARNING: Data streaming is PAUSED. Click 'Toggle Stream' button to resume.")
        
        # Parse all tuning values and stats OUTSIDE the lock
        # (tuning_updates and comm_updates already initialized at start of function)
        
        # Parse I2C stats: 📊 I2C Stats: Success=1234 (99.5%), Fail=6, Total=1240
        match = re.search(r'I2C Stats: Success=(\d+)\s+\(([\d.]+)%\),\s+Fail=(\d+)', line)
        if match:
            comm_updates['imu_success'] = int(match.group(1))
            comm_updates['imu_fail'] = int(match.group(3))
            rate = float(match.group(2))
            comm_updates['imu_rate'] = rate
            comm_updates['imu_hz'] = 400.0 * (rate / 100.0)
        
        # Parse VESC stats: 📊 VESC Stats: Success=567 (98.2%), Fail=10
        match = re.search(r'VESC Stats: Success=(\d+)\s+\(([\d.]+)%\),\s+Fail=(\d+)', line)
        if match:
            comm_updates['vesc_success'] = int(match.group(1))
            comm_updates['vesc_fail'] = int(match.group(3))
            rate = float(match.group(2))
            comm_updates['vesc_rate'] = rate
            comm_updates['vesc_hz'] = 67.0 * (rate / 100.0)
        
        # Parse tuning values (do all regex outside lock)
        # ORDER IS CRITICAL: Most specific patterns FIRST to prevent false matches
        for pattern, key in [
            # Velocity PID parsing - MUST come FIRST (most specific)
            # Use word boundaries and ensure "Velocity" is matched as whole word to prevent "Vel" substring matches
            (r'\bVelocity\s+Kp\s*=\s*([\d.]+)', 'Kp_vel'),  # Matches "Velocity Kp = X.XX" (tuning command response)
            (r'\bVelocity\s+Ki\s*=\s*([\d.]+)', 'Ki_vel'),  # Matches "Velocity Ki = X.XX" (tuning command response)
            (r'\bVelocity\s+Kd\s*=\s*([\d.]+)', 'Kd_vel'),  # Matches "Velocity Kd = X.XX" (tuning command response)
            (r'\bVelocity\s+Kp:\s+([\d.]+)', 'Kp_vel'),  # Matches "Velocity Kp: X.XX" (printTuningValues)
            (r'\bVelocity\s+Ki:\s+([\d.]+)', 'Ki_vel'),  # Matches "Velocity Ki: X.XX" (printTuningValues)
            (r'\bVelocity\s+Kd:\s+([\d.]+)', 'Kd_vel'),  # Matches "Velocity Kd: X.XX" (printTuningValues)
            # Short form "Vel Kp" - must have word boundary before "Vel" to prevent matching "Velocity"
            (r'\bVel\s+Kp\s*=\s*([\d.]+)', 'Kp_vel'),  # Short form "Vel Kp = X.XX" (must be separate word, not substring)
            (r'\bVel\s+Ki\s*=\s*([\d.]+)', 'Ki_vel'),  # Short form "Vel Ki = X.XX"
            (r'\bVel\s+Kd\s*=\s*([\d.]+)', 'Kd_vel'),  # Short form "Vel Kd = X.XX"
            # Velocity debug line format: 🔍 VEL: ... [Kp=X.XXX Ki=X] - MUST come before generic patterns
            (r'🔍\s+VEL:.*\[Kp=([\d.]+)\s+Ki=([\d.]+)\]', None),  # Special case - parse both Kp_vel and Ki_vel from debug line
            # Angle PID parsing - MUST come BEFORE generic patterns
            (r'\bRoll Kp\s*=\s*([\d.]+)', 'Kp'),  # Matches "Roll Kp = X.XX" (tuning command response)
            (r'\bRoll Ki\s*=\s*([\d.]+)', 'Ki'),  # Matches "Roll Ki = X.XX" (tuning command response)
            (r'\bRoll Kd\s*=\s*([\d.]+)', 'Kd'),  # Matches "Roll Kd = X.XX" (tuning command response)
            (r'\bRoll Kp:\s+([\d.]+)', 'Kp'),  # Matches "Roll Kp: X.XX" (printTuningValues)
            (r'\bRoll Ki:\s+([\d.]+)', 'Ki'),  # Matches "Roll Ki: X.XX" (printTuningValues)
            (r'\bRoll Kd:\s+([\d.]+)', 'Kd'),  # Matches "Roll Kd: X.XX" (printTuningValues)
            (r'\bAngle Kp\s*=\s*([\d.]+)', 'Kp'),  # Matches "Angle Kp = X.XX" (tuning command response)
            (r'\bAngle Ki\s*=\s*([\d.]+)', 'Ki'),  # Matches "Angle Ki = X.XX" (tuning command response)
            (r'\bAngle Kd\s*=\s*([\d.]+)', 'Kd'),  # Matches "Angle Kd = X.XX" (tuning command response)
            (r'\bAngle Kp:\s+([\d.]+)', 'Kp'),  # Matches "Angle Kp: X.XX" (printTuningValues format)
            (r'\bAngle Ki:\s+([\d.]+)', 'Ki'),  # Matches "Angle Ki: X.XX" (printTuningValues format)
            (r'\bAngle Kd:\s+([\d.]+)', 'Kd'),  # Matches "Angle Kd: X.XX" (printTuningValues format)
            (r'Roll Gains:\s+Kp=([\d.]+),\s+Ki=([\d.]+),\s+Kd=([\d.]+)', None),  # Special case for combined
            # Yaw PID parsing - MUST come BEFORE generic Kp/Ki/Kd patterns to avoid false matches
            (r'\bYaw Kp\s*=\s*([\d.]+)', 'Kp_yaw'),  # Matches "Yaw Kp = 0.50" (tuning command response)
            (r'\bYaw Ki\s*=\s*([\d.]+)', 'Ki_yaw'),  # Matches "Yaw Ki = 0.00" (tuning command response)
            (r'\bYaw Kd\s*=\s*([\d.]+)', 'Kd_yaw'),  # Matches "Yaw Kd = 0.10" (tuning command response)
            (r'\bKp_yaw:\s+([\d.]+)', 'Kp_yaw'),  # Matches "Kp_yaw: X.XX" (printTuningValues)
            (r'\bKi_yaw:\s+([\d.]+)', 'Ki_yaw'),  # Matches "Ki_yaw: X.XX" (printTuningValues)
            (r'\bKd_yaw:\s+([\d.]+)', 'Kd_yaw'),  # Matches "Kd_yaw: X.XX" (printTuningValues)
            (r'\bKp_yaw\s*=\s*([\d.]+)', 'Kp_yaw'),  # Fallback pattern
            (r'\bKi_yaw\s*=\s*([\d.]+)', 'Ki_yaw'),  # Fallback pattern
            (r'\bKd_yaw\s*=\s*([\d.]+)', 'Kd_yaw'),  # Fallback pattern
            # Explicit key-value patterns (most specific)
            (r'Kp_angle:\s+([\d.]+)', 'Kp_angle'),
            (r'Ki_angle:\s+([\d.]+)', 'Ki_angle'),
            (r'Kd_angle:\s+([\d.]+)', 'Kd_angle'),
            (r'Kp_vel:\s+([\d.]+)', 'Kp_vel'),
            (r'Ki_vel:\s+([\d.]+)', 'Ki_vel'),
            (r'Kd_vel:\s+([\d.]+)', 'Kd_vel'),
            # Other specific patterns
            (r'Max Current:\s+([\d.]+)A', 'max_current'),
            (r'Max Current\s*=\s*([\d.]+)A', 'max_current'),
            (r'Angle Filter Alpha\s*=\s*([\d.]+)', 'angle_filter_alpha'),
            (r'Angle Filter Alpha:\s+([\d.]+)', 'angle_filter_alpha'),
            (r'Angle Setpoint:\s+([-\d.]+)', 'angle_setpoint'),
            (r'Base Angle Setpoint:\s+([-\d.]+)°', 'angle_setpoint'),
            (r'Angle Setpoint\s*=\s*([-\d.]+)°', 'angle_setpoint'),
            (r'Deadband:\s+([\d.]+)', 'deadband'),
            (r'Deadband\s*=\s*([\d.]+)°', 'deadband'),
            (r'Drive Offset:\s+([-\d.]+)°', 'drive_offset'),
            (r'Drive Offset\s*=\s*([-\d.]+)°', 'drive_offset'),
            (r'Yaw Setpoint:\s+([-\d.]+)°', 'yaw_setpoint'),
            (r'Yaw Control:\s+(\w+)', 'yaw_control_enabled'),  # Will parse "ENABLED" or "DISABLED"
            (r'useVelocityLoop:\s+(\w+)', 'use_vel_loop'),     # "ON" or "OFF"
            (r'Velocity loop\s+(\w+)', 'use_vel_loop'),        # "ENABLED" or "DISABLED" (after pressing v)
            (r'Motor Output:\s+(\w+)', 'motor_output_enabled'), # "ENABLED" or "DISABLED"
            (r'Motor output\s+(\w+)', 'motor_output_enabled'),  # "ENABLED" or "DISABLED" (after pressing o)
            # Legacy patterns for backward compatibility (deprecated - should not match if Roll/Yaw/Velocity labels are present)
            # Use negative lookbehind to prevent matching if Roll/Yaw/Velocity prefix exists
            # NOTE: Lookbehind must be fixed-width, so we use single space \s (not \s+)
            (r'(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )Kp:\s+([\d.]+)\s+Ki:\s+([\d.]+)\s+Kd:\s+([\d.]+)', None),  # Special case - only if no prefix
            (r'^\s+(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )Kp:\s+([\d.]+)', 'Kp'),  # Only at start of line, no prefix
            (r'^\s+(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )Ki:\s+([\d.]+)', 'Ki'),  # Only at start of line, no prefix
            (r'^\s+(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )Kd:\s+([\d.]+)', 'Kd'),  # Only at start of line, no prefix
            # Generic patterns with negative lookbehind to prevent matching Roll/Yaw/Velocity variants (LAST RESORT)
            # IMPORTANT: Must check for "Velocity" and "Vel" as separate words to prevent false matches
            # CRITICAL: Exclude velocity debug line format [Kp=... Ki=...] - use negative lookahead
            # Use fixed-width lookbehind (single space) - Python regex requires fixed-width
            (r'(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )(?!.*\[Kp=)\bKp\s*=\s*([\d.]+)', 'Kp'),  # Exclude [Kp=...] in brackets (velocity debug)
            (r'(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )(?!.*\[Ki=)\bKi\s*=\s*([\d.]+)', 'Ki'),  # Exclude [Ki=...] in brackets (velocity debug)
            (r'(?<!Roll )(?<!Yaw )(?<!Velocity )(?<!Vel )\bKd\s*=\s*([\d.]+)', 'Kd'),  # Negative lookbehind prevents prefixes from matching
        ]:
            match = re.search(pattern, line)
            if match:
                if key is None:  # Special case for combined Kp/Ki/Kd or velocity debug line
                    # Check if this is the velocity debug line format
                    if '🔍' in pattern or 'VEL:' in pattern:
                        # Velocity debug line: [Kp=X.XXX Ki=X] -> extract Kp_vel and Ki_vel
                        tuning_updates['Kp_vel'] = float(match.group(1))
                        tuning_updates['Ki_vel'] = float(match.group(2))
                    else:
                        # Roll Gains format: Kp=X, Ki=Y, Kd=Z -> extract Angle Kp, Ki, Kd
                        tuning_updates['Kp'] = float(match.group(1))
                        tuning_updates['Ki'] = float(match.group(2))
                        tuning_updates['Kd'] = float(match.group(3))
                    break  # Stop after first match for special cases
                elif key == 'yaw_control_enabled':
                    # Parse "ENABLED" or "DISABLED" to boolean
                    tuning_updates[key] = (match.group(1).upper() == "ENABLED")
                    break  # Stop after first match
                elif key == 'use_vel_loop':
                    # Parse "ON"/"OFF" or "ENABLED"/"DISABLED" to boolean
                    raw = match.group(1).upper()
                    tuning_updates[key] = (raw == "ON" or raw == "ENABLED")
                    break
                elif key == 'motor_output_enabled':
                    # Parse "ENABLED"/"DISABLED" to boolean
                    raw = match.group(1).upper()
                    tuning_updates[key] = (raw == "ENABLED" or raw == "ON")
                    break
                else:
                    tuning_updates[key] = float(match.group(1))
                    break  # CRITICAL: Stop after first match to prevent multiple patterns from overwriting
        
        # Parse control direction settings (no lock needed for boolean flags)
        match = re.search(r'Motor Directions:\s+(\w+)', line)
        if match:
            with self.lock:
                self.motor_directions_swapped = (match.group(1) == "SWAPPED")
        
        match = re.search(r'Roll Sign:\s+(\w+)', line)
        if match:
            with self.lock:
                self.roll_sign_inverted = (match.group(1) == "INVERTED")
        
        # Now update data structures with minimal lock time
        if tuning_updates or comm_updates:
            with self.lock:
                if tuning_updates:
                    self.tuning_values.update(tuning_updates)
                if comm_updates:
                    self.comm_stats.update(comm_updates)
    
    def _parse_sync_data(self, line):
        """Parse SYNC line from firmware and update all tuning values
        Format: SYNC:Kp=1.50,Ki=0.00,Kd=0.03,Kp_vel=1.00,Ki_vel=0.00,Kd_vel=0.00,
                Kp_yaw=0.00,Ki_yaw=0.00,Kd_yaw=0.00,setpoint=-0.70,maxCurrent=6.50,
                velSetpoint=0.00,yawEnabled=1,fineAdjust=0
        """
        self._debug_log(f"Parsing SYNC data: {line[:80]}...")
        
        # Remove "SYNC:" prefix
        data_str = line[5:].strip()
        
        # Parse key=value pairs
        sync_updates = {}
        for pair in data_str.split(','):
            if '=' in pair:
                key, value = pair.split('=', 1)
                key = key.strip()
                value = value.strip()
                
                # Map firmware keys to GUI tuning_values keys
                key_map = {
                    'Kp': 'Kp',
                    'Ki': 'Ki',
                    'Kd': 'Kd',
                    'Kp_vel': 'Kp_vel',
                    'Ki_vel': 'Ki_vel',
                    'Kd_vel': 'Kd_vel',
                    'Kp_yaw': 'Kp_yaw',
                    'Ki_yaw': 'Ki_yaw',
                    'Kd_yaw': 'Kd_yaw',
                    'setpoint': 'angle_setpoint',
                    'maxCurrent': 'max_current',
                    'angleFilterAlpha': 'angle_filter_alpha',
                    'velSetpoint': 'velocity_setpoint',
                    'yawEnabled': 'yaw_control_enabled',
                    'fineAdjust': 'fine_adjust',
                    'useVelLoop': 'use_vel_loop',
                    'leftMotorSign': 'left_motor_sign',
                    'rightMotorSign': 'right_motor_sign',
                    'leftVelSign': 'left_vel_sign',
                    'rightVelSign': 'right_vel_sign'
                }
                
                gui_key = key_map.get(key)
                if gui_key:
                    try:
                        # Handle boolean values
                        if gui_key in ['yaw_control_enabled', 'fine_adjust']:
                            sync_updates[gui_key] = (value == '1')
                        elif gui_key == 'use_vel_loop':
                            sync_updates[gui_key] = (value == '1')
                        else:
                            sync_updates[gui_key] = float(value)
                    except ValueError:
                        self._debug_log(f"Could not parse SYNC value: {key}={value}", "WARNING")
        
        # Update tuning values with sync data
        if sync_updates:
            with self.lock:
                self.tuning_values.update(sync_updates)
            self._debug_log(f"SYNC complete: Updated {len(sync_updates)} parameters")
            print(f"✓ Synced {len(sync_updates)} parameters from firmware")
    
    def request_sync(self):
        """Request firmware to send current parameter values"""
        if self.connected and self.ser:
            try:
                self._debug_log("Requesting parameter sync from firmware")
                self.ser.write(b'@')
                self.ser.flush()
                print("📡 Requesting parameter sync from firmware...")
            except Exception as e:
                self._debug_log(f"Failed to send sync request: {e}", "ERROR")
    
    def get_data(self):
        """Get current data (thread-safe)"""
        with self.lock:
            return {
                'imu': self.imu_data.copy(),
                'comm': self.comm_stats.copy(),
                'tuning': self.tuning_values.copy(),
                'plot': {k: list(v) for k, v in self.plot_data.items()},
                'connected': self.connected,
                'last_update': self.last_data_time,
                'roll_sign_inverted': self.roll_sign_inverted,
                'motor_directions_swapped': self.motor_directions_swapped
            }


class RobotTuningGUI:
    """Main GUI application"""
    def __init__(self, root, scale=1.0):
        self.root = root
        self.root.title("Robot Tuning GUI - Self-Balancing Robot")
        self._ui_scale = scale
        
        # Determine font scale based on screen resolution and DPI
        self._setup_scaling()
        
        # Configure ttk.Style for ttk widgets (ttk buttons don't support font directly)
        self.style = ttk.Style()
        self.style.configure("Large.TButton", font=self.medium_font, padding=(8, 4))
        self.style.configure("Medium.TButton", font=self.medium_font, padding=(6, 3))
        self.style.configure("TLabel", font=self.default_font)
        self.style.configure("TLabelframe.Label", font=self.medium_font)
        self.style.configure("TCheckbutton", font=self.default_font)
        self.style.configure("TCombobox", font=self.default_font)
        
    def _setup_scaling(self):
        """Configure fonts and geometry to look good on both standard and HiDPI displays."""
        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()
        
        # Use tkinter's current scaling (already adjusted by detect_scale_factor in main)
        tk_scale = self.root.tk.call('tk', 'scaling')
        # tk scaling of ~1.33 is "normal" (96 DPI); higher means HiDPI was detected
        # We use font points which should scale with tk scaling, but we also
        # bump base sizes to be comfortable on large monitors
        
        # Determine base font size from resolution, then apply user scale
        base_size = 11
        if screen_w >= 2560 or screen_h >= 1600:
            base_size = 12
        if screen_w >= 3200 or screen_h >= 2000:
            base_size = 13
        
        # Apply user-supplied scale factor (--scale flag or 1.0 default)
        ui_scale = getattr(self, '_ui_scale', 1.0)
        base_size = max(7, round(base_size * ui_scale))
        
        self.default_font = ("DejaVu Sans", base_size)
        self.large_font = ("DejaVu Sans", base_size + 6, "bold")
        self.medium_font = ("DejaVu Sans", base_size + 2)
        self.small_font = ("DejaVu Sans", base_size - 1)
        self.button_font = ("DejaVu Sans", base_size + 1)
        
        # Window geometry: use a reasonable fraction of the screen
        win_w = min(int(screen_w * 0.75), 1800)
        win_h = min(int(screen_h * 0.80), 1100)
        # Ensure minimum usable size
        win_w = max(win_w, 1200)
        win_h = max(win_h, 750)
        # Center on screen
        x = (screen_w - win_w) // 2
        y = max(0, (screen_h - win_h) // 2 - 30)
        self.root.geometry(f"{win_w}x{win_h}+{x}+{y}")
        
        # Store scale info for widgets that need manual sizing
        self.base_font_size = base_size
        
        print(f"Display: {screen_w}x{screen_h}, tk_scaling={tk_scale:.2f}, ui_scale={ui_scale:.2f}x, font_base={base_size}pt, window={win_w}x{win_h}")
    
        # Set up log directory (in same directory as script)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        log_dir = os.path.join(script_dir, "logs")
        self.serial_reader = SerialReader(log_dir=log_dir)
        self.serial_reader.gui = self  # Allow SerialReader to update GUI elements
        self.log_dir = log_dir
        self.update_job = None
        
        self.setup_ui()
        self.refresh_devices()
        self.start_updates()
    
    def setup_ui(self):
        """Setup the user interface with improved layout"""
        pad = 5  # Standard padding
        
        # Top toolbar: Connection controls
        toolbar = ttk.Frame(self.root, padding=str(pad))
        toolbar.pack(fill=tk.X)
        
        # Device selection (left side)
        ttk.Label(toolbar, text="Device:", font=self.medium_font).pack(side=tk.LEFT, padx=pad)
        self.device_var = tk.StringVar()
        self.device_combo = ttk.Combobox(toolbar, textvariable=self.device_var, width=35, state="readonly", font=self.default_font)
        self.device_combo.pack(side=tk.LEFT, padx=pad)
        self.device_combo.bind('<<ComboboxSelected>>', self._on_device_selected)
        
        tk.Button(toolbar, text="Refresh", command=self.refresh_devices, width=10,
                  font=self.button_font, padx=6, pady=3).pack(side=tk.LEFT, padx=pad)
        self.connect_btn = tk.Button(toolbar, text="Connect", command=self.toggle_connection, width=12,
                                     font=self.button_font, padx=6, pady=3)
        self.connect_btn.pack(side=tk.LEFT, padx=pad)
        
        # Status indicators (right side)
        self.status_label = ttk.Label(toolbar, text="Disconnected", foreground="red", font=self.medium_font)
        self.status_label.pack(side=tk.LEFT, padx=pad * 3)
        
        self.log_status_label = ttk.Label(toolbar, text="Logging: OFF", foreground="gray", font=self.medium_font)
        self.log_status_label.pack(side=tk.LEFT, padx=pad)
        
        tk.Button(toolbar, text="Toggle Log", command=self.toggle_logging, width=12,
                  font=self.button_font, padx=6, pady=3).pack(side=tk.LEFT, padx=pad)
        
        # Status message area (for parameter update feedback)
        self.status_msg_label = ttk.Label(toolbar, text="", foreground="gray", font=self.small_font, width=30)
        self.status_msg_label.pack(side=tk.LEFT, padx=pad * 2)
        self.status_msg_timeout = None
        
        # Main content area - use grid for better control
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=pad, pady=pad)
        
        # Left side: IMU Data and Plot (takes more space)
        left_frame = ttk.LabelFrame(main_frame, text="IMU Data & Monitoring", padding="10")
        left_frame.grid(row=0, column=0, sticky="nsew", padx=pad)
        main_frame.columnconfigure(0, weight=2)  # Left side gets 2x space
        main_frame.rowconfigure(0, weight=1)
        
        # Top section: IMU values in a grid for compact display
        imu_grid = ttk.Frame(left_frame)
        imu_grid.pack(fill=tk.X, pady=pad)
        
        # Roll
        ttk.Label(imu_grid, text="Roll:", font=self.medium_font, width=8).grid(row=0, column=0, sticky=tk.W, padx=2)
        self.roll_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.roll_label.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        # Pitch
        ttk.Label(imu_grid, text="Pitch:", font=self.medium_font, width=8).grid(row=0, column=2, sticky=tk.W, padx=2)
        self.pitch_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.pitch_label.grid(row=0, column=3, sticky=tk.W, padx=5)
        
        # Yaw
        ttk.Label(imu_grid, text="Yaw:", font=self.medium_font, width=8).grid(row=1, column=0, sticky=tk.W, padx=2)
        self.yaw_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.yaw_label.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        # Setpoint info (target angle and error)
        ttk.Label(imu_grid, text="Target:", font=self.medium_font, width=8).grid(row=1, column=2, sticky=tk.W, padx=2)
        self.setpoint_info_label = ttk.Label(imu_grid, text="--", font=self.medium_font, width=12, anchor=tk.W, foreground="blue")
        self.setpoint_info_label.grid(row=1, column=3, sticky=tk.W, padx=5)
        
        # Balance status (prominent)
        ttk.Label(imu_grid, text="Balance:", font=self.medium_font, width=8).grid(row=2, column=0, sticky=tk.W, padx=2)
        self.balance_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.balance_label.grid(row=2, column=1, sticky=tk.W, padx=5)
        
        # Communication stats (compact, side by side)
        comm_frame = ttk.Frame(left_frame)
        comm_frame.pack(fill=tk.X, pady=3)
        
        self.imu_comm_label = ttk.Label(comm_frame, text="IMU: -- Hz (--%)", font=self.medium_font)
        self.imu_comm_label.pack(side=tk.LEFT, padx=10)
        self.vesc_comm_label = ttk.Label(comm_frame, text="VESC: -- Hz (--%)", font=self.medium_font)
        self.vesc_comm_label.pack(side=tk.LEFT, padx=10)
        
        # Plot (takes remaining space)
        self.setup_plot(left_frame)
        
        # Right side: Tuning Parameters - scrollable so controls are never cut off
        right_outer = ttk.LabelFrame(main_frame, text="Tuning Parameters", padding="4")
        right_outer.grid(row=0, column=1, sticky="nsew", padx=pad)
        main_frame.columnconfigure(1, weight=1)

        tuning_canvas = tk.Canvas(right_outer, highlightthickness=0)
        tuning_scrollbar = ttk.Scrollbar(right_outer, orient="vertical", command=tuning_canvas.yview)
        tuning_canvas.configure(yscrollcommand=tuning_scrollbar.set)
        tuning_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        tuning_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        tuning_inner = ttk.Frame(tuning_canvas)
        tuning_win_id = tuning_canvas.create_window((0, 0), window=tuning_inner, anchor="nw")

        def _on_tuning_resize(event):
            tuning_canvas.configure(scrollregion=tuning_canvas.bbox("all"))
        tuning_inner.bind("<Configure>", _on_tuning_resize)

        def _on_canvas_resize(event):
            tuning_canvas.itemconfig(tuning_win_id, width=event.width)
        tuning_canvas.bind("<Configure>", _on_canvas_resize)

        # Mouse-wheel scroll: Button-4/5 for Linux, MouseWheel for Windows/macOS
        tuning_canvas.bind("<Button-4>", lambda e: tuning_canvas.yview_scroll(-1, "units"))
        tuning_canvas.bind("<Button-5>", lambda e: tuning_canvas.yview_scroll(1, "units"))
        tuning_canvas.bind("<MouseWheel>", lambda e: tuning_canvas.yview_scroll(-1 * (e.delta // 120), "units"))

        self.setup_tuning_controls(tuning_inner)
        
        # Bottom toolbar: Action buttons
        action_frame = ttk.Frame(self.root, padding=str(pad))
        action_frame.pack(fill=tk.X)
        
        for text, cmd, width in [
            ("Show All Values", self.show_tuning_values, 16),
            ("Sync Params", self.sync_params, 13),
            ("Load Settings", lambda: self.serial_reader.send_command('g'), 14),
            ("Toggle Stream", lambda: self.serial_reader.send_command(' '), 14),
            ("Open Logs", self.open_log_folder, 12),
            ("Help", self.show_help, 10),
        ]:
            tk.Button(action_frame, text=text, command=cmd, width=width,
                      font=self.button_font, padx=6, pady=4).pack(side=tk.LEFT, padx=pad)

        tk.Button(action_frame, text="💾 Save Settings (k)", width=18,
                  command=lambda: self.serial_reader.send_command('k'),
                  font=self.button_font, padx=6, pady=4,
                  background="#27ae60", foreground="white",
                  activebackground="#1e8449", activeforeground="white"
                  ).pack(side=tk.LEFT, padx=pad)
    
    def setup_plot(self, parent):
        """Setup matplotlib plot with DPI-aware sizing"""
        # Use 100 DPI for sharper rendering on HiDPI; matplotlib respects this
        plot_dpi = 100
        plot_fontsize = max(10, self.base_font_size - 1)
        
        self.fig, self.ax = plt.subplots(figsize=(7, 4.5), dpi=plot_dpi)
        self.ax.set_xlabel("Time (s)", fontsize=plot_fontsize)
        self.ax.set_ylabel("Value", fontsize=plot_fontsize)
        self.ax.set_title("Real-time Data", fontsize=plot_fontsize + 2, fontweight='bold')
        self.ax.grid(True, alpha=0.3)
        self.ax.tick_params(labelsize=plot_fontsize - 1)
        
        self.line_roll, = self.ax.plot([], [], label="Roll (°)", color='blue', linewidth=2)
        self.line_pitch, = self.ax.plot([], [], label="Pitch (°)", color='green', linewidth=2)
        self.line_current, = self.ax.plot([], [], label="Current (A)", color='red', linewidth=2)
        self.ax.legend(fontsize=plot_fontsize - 1, loc='upper right')
        
        self.canvas = FigureCanvasTkAgg(self.fig, parent)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, pady=4)
    
    def setup_tuning_controls(self, parent):
        """Setup tuning parameter controls with larger, more readable layout"""
        # Initialize param_labels dictionary first
        self.param_labels = {}
        self.last_param_values = {}  # Track previous values for change detection
        self.fine_adjust_enabled = tk.BooleanVar(value=False)
        self.fine_adjust_state = False
        
        # Angle PID Control (Inner Loop)
        pid_frame = ttk.LabelFrame(parent, text="Angle PID Control (Inner Loop)", padding="8")
        pid_frame.pack(fill=tk.X, pady=4)
        
        self.create_param_control(pid_frame, "Kp", 'p', 'P', 'Kp', 0.5)
        self.create_param_control(pid_frame, "Ki", 'i', 'I', 'Ki', 0.05)
        # Note: 'd' toggles diagnostic mode, 'j' decreases Kd, 'D' increases Kd
        self.create_param_control(pid_frame, "Kd", 'j', 'D', 'Kd', 0.05)
        self.create_param_control(pid_frame, "Filter Alpha", 'a', 'A', 'angle_filter_alpha', 0.05)
        
        fine_frame = ttk.Frame(pid_frame)
        fine_frame.pack(fill=tk.X, pady=3)
        ttk.Label(fine_frame, text="Fine Adjust:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(
            fine_frame,
            text="Enable (smaller steps)",
            variable=self.fine_adjust_enabled,
            command=self.toggle_fine_adjust
        ).pack(side=tk.LEFT, padx=3)
        
        # Diagnostic Mode Toggle
        diag_frame = ttk.Frame(pid_frame)
        diag_frame.pack(fill=tk.X, pady=3)
        ttk.Label(diag_frame, text="Mode:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        self.mode_label = ttk.Label(diag_frame, text="PID", font=self.medium_font, width=12, anchor=tk.CENTER)
        self.mode_label.pack(side=tk.LEFT, padx=6)
        tk.Button(diag_frame, text="Toggle (d)", font=self.button_font, padx=8, pady=2,
                 command=lambda: self.serial_reader.send_command('d')).pack(side=tk.LEFT, padx=3)
        
        # Yaw PID Control (Rotation Control)
        yaw_frame = ttk.LabelFrame(parent, text="Yaw PID Control (Rotation)", padding="8")
        yaw_frame.pack(fill=tk.X, pady=4)
        
        self.create_param_control(yaw_frame, "Yaw Kp", 'y', 'Y', 'Kp_yaw', 0.5)
        self.create_param_control(yaw_frame, "Yaw Ki", 'u', 'U', 'Ki_yaw', 0.05)
        self.create_param_control(yaw_frame, "Yaw Kd", 'h', 'H', 'Kd_yaw', 0.05)
        
        yaw_toggle_frame = ttk.Frame(yaw_frame)
        yaw_toggle_frame.pack(fill=tk.X, pady=3)
        ttk.Label(yaw_toggle_frame, text="Yaw Control:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        self.yaw_control_label = ttk.Label(yaw_toggle_frame, text="ENABLED", font=self.medium_font, width=12, anchor=tk.CENTER)
        self.yaw_control_label.pack(side=tk.LEFT, padx=6)
        tk.Button(yaw_toggle_frame, text="Toggle (n)", font=self.button_font, padx=8, pady=2,
                 command=lambda: self.serial_reader.send_command('n')).pack(side=tk.LEFT, padx=3)
        
        # Velocity Control (Phase 1: Cascaded Control)
        velocity_frame = ttk.LabelFrame(parent, text="Velocity Control (Phase 1)", padding="8")
        velocity_frame.pack(fill=tk.X, pady=4)
        
        # Velocity Setpoint Control
        self.create_param_control(velocity_frame, "Vel Setpoint (m/s)", '6', 'V', 'velocity_setpoint', 0.1)

        # Velocity loop enable/disable toggle
        vel_loop_frame = ttk.Frame(velocity_frame)
        vel_loop_frame.pack(fill=tk.X, pady=3)
        ttk.Label(vel_loop_frame, text="Velocity Loop:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        self.velocity_loop_status_label = ttk.Label(vel_loop_frame, text="OFF", font=self.medium_font, width=8, anchor=tk.CENTER)
        self.velocity_loop_status_label.pack(side=tk.LEFT, padx=6)
        tk.Button(
            vel_loop_frame,
            text="Toggle (v)",
            font=self.button_font,
            padx=8,
            pady=2,
            command=lambda: self.serial_reader.send_command('v')
        ).pack(side=tk.LEFT, padx=3)
        
        # Current Velocity Display
        vel_display_frame = ttk.Frame(velocity_frame)
        vel_display_frame.pack(fill=tk.X, pady=3)
        ttk.Label(vel_display_frame, text="Current Velocity:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        self.velocity_display_label = ttk.Label(vel_display_frame, text="0.00 m/s", font=self.medium_font, width=12, anchor=tk.CENTER)
        self.velocity_display_label.pack(side=tk.LEFT, padx=6)
        
        # Velocity Stop Button
        tk.Button(vel_display_frame, text="Stop (0)", font=self.button_font, padx=8, pady=2,
                 command=lambda: self.serial_reader.send_command('0')).pack(side=tk.LEFT, padx=3)
        
        # Velocity PID Tuning (P-only for initial tuning, Kd always 0)
        self.create_param_control(velocity_frame, "Vel Kp", 'w', 'W', 'Kp_vel', 0.05)
        self.create_param_control(velocity_frame, "Vel Ki", 'e', 'E', 'Ki_vel', 0.01)
        # Note: Keep Ki=0 for initial tuning (P-only). Vel Kd is always 0 (PI only controller)
        vel_kd_frame = ttk.Frame(velocity_frame)
        vel_kd_frame.pack(fill=tk.X, pady=3)
        ttk.Label(vel_kd_frame, text="Vel Kd:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        vel_kd_label = ttk.Label(vel_kd_frame, text="0.00 (PI only)", font=self.medium_font, width=12, anchor=tk.CENTER)
        vel_kd_label.pack(side=tk.LEFT, padx=6)
        self.param_labels['Kd_vel'] = vel_kd_label  # Display only, not adjustable
        
        # Motor Settings
        motor_frame = ttk.LabelFrame(parent, text="Motor Settings", padding="8")
        motor_frame.pack(fill=tk.X, pady=4)
        
        self.create_param_control(motor_frame, "Max Current (A)", 'm', 'M', 'max_current', 0.5)
        # Angle Setpoint: Target roll angle for balance (typically -3° to +3°, not necessarily 0°)
        self.create_param_control(motor_frame, "Angle Setpoint (°)", 'z', 'Z', 'angle_setpoint', 0.1)
        
        # Motor output enable/disable (dry-run mode)
        motor_out_frame = ttk.Frame(motor_frame)
        motor_out_frame.pack(fill=tk.X, pady=3)
        ttk.Label(motor_out_frame, text="Motor Output:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        self.motor_output_status_label = ttk.Label(motor_out_frame, text="ENABLED", font=self.medium_font, width=12, anchor=tk.CENTER)
        self.motor_output_status_label.pack(side=tk.LEFT, padx=6)
        tk.Button(motor_out_frame, text="Toggle (o)", font=self.button_font, padx=8, pady=2,
                 command=lambda: self.serial_reader.send_command('o')).pack(side=tk.LEFT, padx=3)
    
    def create_param_control(self, parent, label, dec_cmd, inc_cmd, param_key, step):
        """Create a parameter control row with larger, more readable controls"""
        frame = ttk.Frame(parent)
        frame.pack(fill=tk.X, pady=2)
        
        # Label
        ttk.Label(frame, text=f"{label}:", font=self.medium_font, width=18).pack(side=tk.LEFT, padx=3)
        
        # Value display — medium font keeps rows compact while still readable
        value_label = ttk.Label(frame, text="--", font=self.medium_font, width=10, anchor=tk.CENTER)
        value_label.pack(side=tk.LEFT, padx=6)
        self.param_labels[param_key] = value_label
        
        # Buttons - use tk.Button for reliable font/size control on all platforms
        btn_font = (self.button_font[0], self.button_font[1] + 2)  # Slightly larger for arrow symbols
        if dec_cmd:
            tk.Button(frame, text=" ▼ ", font=btn_font, padx=8, pady=2,
                     command=lambda: self.adjust_param(dec_cmd, param_key, -step)).pack(side=tk.LEFT, padx=3)
        else:
            # Placeholder to keep alignment
            ttk.Label(frame, text="", width=5).pack(side=tk.LEFT, padx=3)
        
        if inc_cmd:
            tk.Button(frame, text=" ▲ ", font=btn_font, padx=8, pady=2,
                     command=lambda: self.adjust_param(inc_cmd, param_key, step)).pack(side=tk.LEFT, padx=3)
    
    def adjust_param(self, cmd, param_key, step):
        """Adjust a parameter"""
        if self.serial_reader.connected:
            # Get current value before change
            data = self.serial_reader.get_data()
            old_value = data['tuning'].get(param_key, 0)
            
            # Send command
            self.serial_reader.send_command(cmd)
            
            # Show status message
            param_name = param_key.replace('_', ' ').title()
            direction = "increased" if step > 0 else "decreased"
            self.show_status_message(f"{param_name} {direction}...", duration=2000)
            
            # Note: Actual value will be updated from serial response
    
    def toggle_fine_adjust(self):
        """Toggle fine adjustment mode in firmware"""
        if self.serial_reader.connected:
            self.serial_reader.send_command('t')
            self.fine_adjust_state = not self.fine_adjust_state
            status = "ON" if self.fine_adjust_state else "OFF"
            self.show_status_message(f"Fine Adjust {status}", duration=1500, color="blue")
    
    def refresh_devices(self):
        """Refresh list of available devices"""
        devices = self.serial_reader.find_devices()
        # Get device info from SerialReader
        self.device_info = getattr(self.serial_reader, 'device_info', {})
        
        # Create display list with friendly names
        display_values = []
        for device in devices:
            if device in self.device_info:
                display_values.append(self.device_info[device])
            else:
                display_values.append(device)
        
        self.device_combo['values'] = display_values
        
        # Store mapping of display name to device path
        self.device_map = {display: device for device, display in zip(devices, display_values)}
        
        if devices:
            self.device_combo.current(0)
            self._on_device_selected()
    
    def _on_device_selected(self, event=None):
        """Handle device selection - update tooltip or status"""
        selection = self.device_var.get()
        if selection in self.device_map:
            device_path = self.device_map[selection]
            # Could add tooltip here if needed
            pass
    
    def toggle_connection(self):
        """Connect or disconnect"""
        if self.serial_reader.connected:
            self.serial_reader.disconnect()
            self.connect_btn.config(text="Connect", bg="SystemButtonFace")
            self.status_label.config(text="Disconnected", foreground="red")
            self.log_status_label.config(text="Logging: OFF", foreground="gray")
        else:
            display_name = self.device_var.get()
            if not display_name:
                messagebox.showerror("Error", "Please select a device")
                return
            
            # Get actual device path from display name
            if display_name in self.device_map:
                device = self.device_map[display_name]
            else:
                # Fallback: try to extract device path from display name
                # Format might be "Teensy usb3/3-1 - /dev/ttyS4"
                if " - " in display_name:
                    device = display_name.split(" - ")[-1]
                else:
                    device = display_name
            
            if self.serial_reader.connect(device):
                self.connect_btn.config(text="Disconnect", bg="#ffcccc")
                self.status_label.config(text="Connected", foreground="green")
                # Update logging status
                if self.serial_reader.log_enabled and self.serial_reader.log_file:
                    log_file = os.path.basename(self.serial_reader.get_log_filename())
                    self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green")
                else:
                    self.log_status_label.config(text="Logging: OFF", foreground="gray")
                # Request current tuning values and ensure data streaming is enabled
                self.serial_reader.send_command('x')
                # Ensure data streaming is enabled (send space twice to guarantee ON state)
                time.sleep(0.1)
                self.serial_reader.send_command(' ')  # Toggle stream
                time.sleep(0.1)
                self.serial_reader.send_command(' ')  # Toggle again to ensure ON
    
    def toggle_logging(self):
        """Toggle logging on/off"""
        enabled = self.serial_reader.toggle_logging()
        if enabled and self.serial_reader.connected:
            log_file = os.path.basename(self.serial_reader.get_log_filename())
            self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green")
        else:
            self.log_status_label.config(text="Logging: OFF", foreground="gray")
    
    def open_log_folder(self):
        """Open the logs folder in file manager"""
        import subprocess
        import platform
        
        if not os.path.exists(self.log_dir):
            messagebox.showinfo("No Logs", "Logs folder doesn't exist yet. Connect to robot to start logging.")
            return
        
        try:
            if platform.system() == "Windows":
                os.startfile(self.log_dir)
            elif platform.system() == "Darwin":  # macOS
                subprocess.run(["open", self.log_dir])
            else:  # Linux
                subprocess.run(["xdg-open", self.log_dir])
        except Exception as e:
            messagebox.showerror("Error", f"Could not open log folder:\n{str(e)}\n\nLogs are in: {self.log_dir}")
    
    def show_tuning_values(self):
        """Show all tuning values"""
        if self.serial_reader.connected:
            self.serial_reader.send_command('x')
        else:
            messagebox.showinfo("Not Connected", "Please connect to robot first")
    
    def sync_params(self):
        """Request parameter sync from firmware to update GUI with actual values"""
        if self.serial_reader.connected:
            self.serial_reader.request_sync()
            self.show_status_message("📡 Syncing parameters from firmware...", duration=2000, color="blue")
        else:
            messagebox.showinfo("Not Connected", "Please connect to robot first")
    
    def show_help(self):
        """Show help dialog"""
        help_text = """Keyboard Shortcuts (also work in GUI):
        
Angle PID (Balance):
  p/P - Decrease/Increase Kp
  i/I - Decrease/Increase Ki
  j/D - Decrease/Increase Kd (NOTE: 'd' toggles diagnostic mode)
  a/A - Decrease/Increase Filter Alpha (0=max smooth, 1=no filter)

Velocity Control (Phase 1 Cascaded):
  v - Toggle Velocity Loop ON/OFF
  6/V - Decrease/Increase Velocity Setpoint (m/s)
  w/W - Decrease/Increase Velocity Kp
  e/E - Decrease/Increase Velocity Ki
  r/R - Velocity Kd (disabled - PI only controller, always 0)
  0 - Stop (set velocity setpoint to 0.0)

Yaw PID (Rotation):
  y/Y - Decrease/Increase Yaw Kp
  u/U - Decrease/Increase Yaw Ki
  h/H - Decrease/Increase Yaw Kd
  n/N - Toggle yaw control on/off

Motor Settings:
  m/M - Decrease/Increase Max Current
  z/Z - Decrease/Increase Angle Setpoint
  t/T - Toggle fine adjust (smaller steps)

Save/Load Settings:
  k/K - Save settings to EEPROM
  g/G - Load settings from EEPROM

Other:
  d - Toggle diagnostic mode
  o - Toggle motor output ON/OFF (dry-run)
  x/X - Show all tuning values
  @ - Sync GUI with firmware parameters
  SPACE - Pause/Resume data stream
  l/L - Start logging
  s/S - Stop logging
  b/B - Download log data (when data available)

GUI Features:
  • Auto-syncs parameters on connect
  • "Sync Params" button to manually refresh
"""
        messagebox.showinfo("Help", help_text)
    
    def start_updates(self):
        """Start the update loop"""
        self.update_display()
        self.update_job = self.root.after(UPDATE_INTERVAL_MS, self.start_updates)
    
    def update_display(self):
        """Update all display elements"""
        data = self.serial_reader.get_data()
        
        # Update connection status (medium font)
        if data['connected']:
            time_since_update = time.time() - data['last_update']
            if data['last_update'] > 0 and time_since_update > 1.0:
                self.status_label.config(text="Connected (No Data)", foreground="orange", font=self.medium_font)
            else:
                self.status_label.config(text="Connected", foreground="green", font=self.medium_font)
            
            # Update logging status
            if self.serial_reader.log_enabled and self.serial_reader.log_file:
                log_file = os.path.basename(self.serial_reader.get_log_filename())
                self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green", font=self.medium_font)
            else:
                self.log_status_label.config(text="Logging: OFF", foreground="gray", font=self.medium_font)
        else:
            self.status_label.config(text="Disconnected", foreground="red", font=self.medium_font)
            self.log_status_label.config(text="Logging: OFF", foreground="gray", font=self.medium_font)
        
        # Update IMU data (larger, more readable)
        imu = data['imu']
        self.roll_label.config(text=f"{imu['roll']:.2f}°", font=self.large_font)
        self.pitch_label.config(text=f"{imu['pitch']:.2f}°", font=self.large_font)
        self.yaw_label.config(text=f"{imu['yaw']:.2f}°", font=self.large_font)
        
        balance_color = "green" if imu['balance_status'] == 'OK' else "red"
        self.balance_label.config(text=f"{imu['balance_status']}", foreground=balance_color, font=self.large_font)
        
        # Update setpoint info (show target vs current with error)
        tuning = data['tuning']
        base_setpoint = tuning.get('angle_setpoint', 0.0)
        drive_offset = tuning.get('drive_offset', 0.0)
        active_setpoint = base_setpoint + drive_offset
        roll = imu['roll']
        error = roll - active_setpoint
        if hasattr(self, 'setpoint_info_label'):
            # Color code: green if error < 0.5°, yellow if < 1.0°, red if > 1.0°
            if abs(error) < 0.5:
                error_color = "green"
            elif abs(error) < 1.0:
                error_color = "orange"
            else:
                error_color = "red"
            self.setpoint_info_label.config(
                text=f"{active_setpoint:.2f}° (err: {error:+.2f}°)", 
                font=self.medium_font,
                foreground=error_color
            )
        
        # Update velocity display
        if hasattr(self, 'velocity_display_label'):
            velocity_actual = imu.get('velocity_actual', 0.0)
            velocity_setpoint = tuning.get('velocity_setpoint', 0.0)
            # Color code: green if close to setpoint, yellow if moderate error, red if large error
            if abs(velocity_actual - velocity_setpoint) < 0.05:
                vel_color = "green"
            elif abs(velocity_actual - velocity_setpoint) < 0.15:
                vel_color = "orange"
            else:
                vel_color = "red"
            self.velocity_display_label.config(
                text=f"{velocity_actual:.2f} m/s",
                foreground=vel_color,
                font=self.medium_font
            )

        # Update drive mode indicator
        if hasattr(self, 'drive_mode_label'):
            if abs(drive_offset) < 0.01:
                drive_text = "STOP"
                drive_color = "gray"
            elif drive_offset > 0:
                drive_text = "FORWARD"
                drive_color = "green"
            else:
                drive_text = "BACK"
                drive_color = "orange"
            self.drive_mode_label.config(text=drive_text, foreground=drive_color, font=self.medium_font)
        
        # Update communication stats (medium font)
        comm = data['comm']
        if comm['imu_hz'] > 0:
            self.imu_comm_label.config(text=f"IMU: {comm['imu_hz']:.0f} Hz ({comm['imu_rate']:.1f}%)", font=self.medium_font)
        else:
            self.imu_comm_label.config(text="IMU: -- Hz (--%)", font=self.medium_font)
        
        if comm['vesc_hz'] > 0:
            self.vesc_comm_label.config(text=f"VESC: {comm['vesc_hz']:.0f} Hz ({comm['vesc_rate']:.1f}%)", font=self.medium_font)
        else:
            self.vesc_comm_label.config(text="VESC: -- Hz (--%)", font=self.medium_font)
        
        # Update tuning parameter labels (large font for readability)
        # Also provide visual feedback when values change
        tuning = data['tuning']
        for key, label in self.param_labels.items():
            value = tuning.get(key, 0)
            old_value = self.last_param_values.get(key, None)
            
            # Format value based on type
            # Note: Kd_vel is always 0 (PI only controller) - display as fixed value
            if key == 'Kd_vel':
                formatted = "0.00 (PI only)"  # Always 0, show as fixed
            elif key in ['Kp_angle', 'Ki_angle', 'Kd_angle', 'Kp_vel', 'Ki_vel', 'Kp', 'Ki', 'Kd', 'Kp_yaw', 'Ki_yaw', 'Kd_yaw', 'angle_filter_alpha']:
                formatted = f"{value:.2f}"
            elif key in ['max_current', 'min_current']:
                formatted = f"{value:.1f}A"
            elif key in ['angle_setpoint', 'drive_offset']:
                formatted = f"{value:.2f}°"
            elif key == 'deadband':
                formatted = f"{value:.2f}°"
            else:
                formatted = f"{value:.3f}"
            
            # Update label
            label.config(text=formatted, font=self.large_font)
            
            # Visual feedback: highlight if value changed
            if old_value is not None and abs(value - old_value) > 0.001:
                # Value changed - briefly highlight in yellow
                label.config(background="yellow", foreground="black")
                # Reset color after 500ms (use system default background)
                self.root.after(500, lambda l=label: l.config(background="", foreground="black"))
                
                # Show confirmation message
                param_name = key.replace('_', ' ').title()
                self.show_status_message(f"✓ {param_name} updated: {formatted}", duration=2000, color="green")
            
            # Store current value for next comparison
            self.last_param_values[key] = value
        
        # Update mode label from parsed IMU data stream
        if hasattr(self, 'mode_label'):
            mode_text = imu.get('control_mode', 'PID')
            mode_color = "red" if mode_text == "DIAG" else "black"
            self.mode_label.config(text=mode_text, foreground=mode_color)
        
        # Update yaw control label
        if hasattr(self, 'yaw_control_label'):
            yaw_enabled = tuning.get('yaw_control_enabled', True)
            yaw_text = "ENABLED" if yaw_enabled else "DISABLED"
            yaw_color = "green" if yaw_enabled else "red"
            self.yaw_control_label.config(text=yaw_text, foreground=yaw_color)
        
        # Update velocity loop status (ON/OFF)
        if hasattr(self, 'velocity_loop_status_label'):
            use_vel_loop = tuning.get('use_vel_loop', False)
            vel_loop_text = "ON" if use_vel_loop else "OFF"
            vel_loop_color = "green" if use_vel_loop else "gray"
            self.velocity_loop_status_label.config(text=vel_loop_text, foreground=vel_loop_color)

        # Update motor output status (dry-run toggle)
        if hasattr(self, 'motor_output_status_label'):
            motor_enabled = tuning.get('motor_output_enabled', True)
            motor_text = "ENABLED" if motor_enabled else "DISABLED"
            motor_color = "green" if motor_enabled else "orange"
            self.motor_output_status_label.config(text=motor_text, foreground=motor_color)
        
        # Sync fine adjust checkbox with firmware state
        if hasattr(self, 'fine_adjust_enabled'):
            firmware_fine = tuning.get('fine_adjust', self.fine_adjust_state)
            if isinstance(firmware_fine, bool) and firmware_fine != self.fine_adjust_state:
                self.fine_adjust_state = firmware_fine
                self.fine_adjust_enabled.set(firmware_fine)
        
        # Update control direction labels
        if hasattr(self, 'roll_sign_label'):
            roll_status = "INVERTED" if data.get('roll_sign_inverted', False) else "NORMAL"
            self.roll_sign_label.config(text=roll_status, font=self.medium_font)
        
        if hasattr(self, 'motor_dir_label'):
            motor_status = "SWAPPED" if data.get('motor_directions_swapped', True) else "ORIGINAL"
            self.motor_dir_label.config(text=motor_status, font=self.medium_font)
        
        # Update plot
        plot_data = data['plot']
        if plot_data['time'] and len(plot_data['time']) > 0:
            self.line_roll.set_data(plot_data['time'], plot_data['roll'])
            self.line_pitch.set_data(plot_data['time'], plot_data['pitch'])
            self.line_current.set_data(plot_data['time'], plot_data['current'])
            
            if len(plot_data['time']) > 0:
                t_max = max(plot_data['time'])
                t_min = max(0, t_max - 10)
                self.ax.set_xlim(t_min, t_max + 1)
                
                # Auto-scale Y axis based on data
                all_data = plot_data['roll'] + plot_data['pitch'] + plot_data['current']
                if all_data:
                    y_min = min(all_data) - 2
                    y_max = max(all_data) + 2
                    self.ax.set_ylim(y_min, y_max)
            
            self.canvas.draw()
    
    def show_status_message(self, message, duration=2000, color="gray"):
        """Show a temporary status message"""
        # Cancel any existing timeout
        if self.status_msg_timeout:
            self.root.after_cancel(self.status_msg_timeout)
        
        # Show message
        self.status_msg_label.config(text=message, foreground=color)
        
        # Clear after duration
        self.status_msg_timeout = self.root.after(duration, lambda: self.status_msg_label.config(text="", foreground="gray"))


def detect_scale_factor(root):
    """Detect display scale factor for HiDPI support.
    
    On Linux with fractional scaling (e.g. Ubuntu 200%), tkinter often ignores
    the system DPI and renders everything tiny. This detects the real situation
    and returns a multiplier for fonts/geometry.
    """
    import platform
    system = platform.system()
    
    try:
        # Get tkinter's idea of screen DPI (default ~96 on Linux, ~72 on macOS)
        screen_dpi = root.winfo_fpixels('1i')
        screen_w = root.winfo_screenwidth()
        screen_h = root.winfo_screenheight()
        
        # On Linux, check for Xft.dpi or GDK_SCALE environment variables
        if system == "Linux":
            # Method 1: Check GDK_SCALE (set by GNOME/Ubuntu scaling)
            gdk_scale = os.environ.get('GDK_SCALE', '')
            if gdk_scale:
                try:
                    return float(gdk_scale)
                except ValueError:
                    pass
            
            # Method 2: Check Xft.dpi via xrdb (common on Ubuntu with scaling)
            try:
                import subprocess
                result = subprocess.run(['xrdb', '-query'], capture_output=True, text=True, timeout=2)
                for line in result.stdout.splitlines():
                    if 'Xft.dpi' in line:
                        xft_dpi = float(line.split(':')[1].strip())
                        # Standard DPI is 96; if Xft.dpi is e.g. 192, scale = 2.0
                        if xft_dpi > 120:
                            return xft_dpi / 96.0
            except Exception:
                pass
            
            # Method 3: Heuristic from screen resolution
            # A 3840x2400 display at 200% reports logical 1920x1200 to apps
            # But if we see the physical resolution is very high, scale up
            if screen_dpi > 120:
                return screen_dpi / 96.0
            
            # Method 4: Check if physical resolution is much larger than expected
            # for the reported screen size (catches Wayland scaling)
            try:
                phys_w = root.winfo_screenmmwidth()
                if phys_w > 0:
                    real_dpi = screen_w / (phys_w / 25.4)
                    if real_dpi > 140:  # HiDPI threshold
                        return max(1.0, real_dpi / 96.0)
            except Exception:
                pass
        
        elif system == "Windows":
            # Windows usually handles DPI scaling via ctypes
            try:
                import ctypes
                ctypes.windll.shcore.SetProcessDpiAwareness(1)
                # Re-query after setting awareness
                screen_dpi = root.winfo_fpixels('1i')
                if screen_dpi > 120:
                    return screen_dpi / 96.0
            except Exception:
                pass
        
        # Default: use tkinter's reported DPI if it suggests scaling
        if screen_dpi > 120:
            return screen_dpi / 96.0
        
    except Exception:
        pass
    
    return 1.0  # No scaling needed


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Robot Tuning GUI - Self-Balancing Robot')
    parser.add_argument(
        '--scale', type=float, default=None,
        help='UI scale factor for font/widget sizes (e.g. 0.75 smaller, 1.5 larger). '
             'Default: auto-detect via HiDPI detection. '
             'Try 0.75 if GUI is too large on Linux, 1.5 if too small on Windows.'
    )
    args = parser.parse_args()

    root = tk.Tk()

    ui_scale = 1.0
    if args.scale is not None:
        ui_scale = args.scale
        print(f"UI scale override: {ui_scale:.2f}x (--scale flag)")
    else:
        # Auto-detect HiDPI and apply to tk's internal scaling
        scale = detect_scale_factor(root)
        if scale > 1.2:
            current_scaling = root.tk.call('tk', 'scaling')
            root.tk.call('tk', 'scaling', current_scaling * scale)
            print(f"HiDPI detected: scale={scale:.1f}x, tk scaling={current_scaling:.1f} -> {current_scaling * scale:.1f}")

    app = RobotTuningGUI(root, scale=ui_scale)
    
    # Ensure proper cleanup on window close
    def on_closing():
        """Handle window close event"""
        if app.serial_reader.connected:
            app.serial_reader.disconnect()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()

