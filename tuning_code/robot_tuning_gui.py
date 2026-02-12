#!/usr/bin/env python3
"""
Robot Tuning GUI - Real-time monitoring and parameter adjustment
for self-balancing robot
"""

import tkinter as tk
from tkinter import ttk, messagebox
import time
import os
import subprocess
import platform
from datetime import datetime

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

from teensy_comms import TeensyComms

# Constants
UPDATE_INTERVAL_MS = 50  # GUI update rate (20 Hz)


class RobotTuningGUI:
    """Main GUI application"""
    def __init__(self, root):
        self.root = root
        self.root.title("Robot Tuning GUI - Self-Balancing Robot")
        self.root.geometry("1400x900")

        self.default_font = ("Arial", 10)
        self.large_font = ("Arial", 16, "bold")
        self.medium_font = ("Arial", 12)
        self.small_font = ("Arial", 9)

        self.style = ttk.Style()
        self.style.configure("Large.TButton", font=self.medium_font)
        self.style.configure("Medium.TButton", font=self.medium_font)

        script_dir = os.path.dirname(os.path.abspath(__file__))
        log_dir = os.path.join(script_dir, "logs")
        self.log_dir = log_dir

        self.serial_reader = TeensyComms(
            log_dir=log_dir,
            on_disconnect=self._on_serial_disconnect,
            on_mode_change=self._on_mode_change,
        )

        self.update_job = None
        self.setup_ui()
        self.refresh_devices()
        self.start_updates()

    # ------------------------------------------------------------------
    # Callbacks from TeensyComms reader thread
    # ------------------------------------------------------------------

    def _on_serial_disconnect(self):
        """Called from reader thread when link drops unexpectedly."""
        self.root.after(0, self._handle_disconnect)

    def _handle_disconnect(self):
        """GUI-thread handler for unexpected disconnect."""
        self.connect_btn.config(text="Connect")
        self.status_label.config(text="Disconnected", foreground="red")
        self.log_status_label.config(text="Logging: OFF", foreground="gray")

    def _on_mode_change(self, mode_str):
        """Called from reader thread when firmware mode token changes."""
        self.root.after(0, lambda m=mode_str: self._update_mode_label(m))

    def _update_mode_label(self, mode_str):
        if hasattr(self, 'mode_label'):
            self.mode_label.config(text=mode_str)

    # ------------------------------------------------------------------
    # UI setup
    # ------------------------------------------------------------------

    def setup_ui(self):
        """Setup the user interface"""
        # Top toolbar
        toolbar = ttk.Frame(self.root, padding="5")
        toolbar.pack(fill=tk.X)

        ttk.Label(toolbar, text="Device:", font=self.medium_font).pack(side=tk.LEFT, padx=3)
        self.device_var = tk.StringVar()
        self.device_combo = ttk.Combobox(toolbar, textvariable=self.device_var, width=35,
                                          state="readonly", font=self.default_font)
        self.device_combo.pack(side=tk.LEFT, padx=3)
        self.device_combo.bind('<<ComboboxSelected>>', self._on_device_selected)

        ttk.Button(toolbar, text="Refresh", command=self.refresh_devices, width=10).pack(side=tk.LEFT, padx=3)
        self.connect_btn = ttk.Button(toolbar, text="Connect", command=self.toggle_connection, width=12)
        self.connect_btn.pack(side=tk.LEFT, padx=3)

        self.status_label = ttk.Label(toolbar, text="Disconnected", foreground="red", font=self.medium_font)
        self.status_label.pack(side=tk.LEFT, padx=15)

        self.log_status_label = ttk.Label(toolbar, text="Logging: OFF", foreground="gray", font=self.medium_font)
        self.log_status_label.pack(side=tk.LEFT, padx=5)

        self.log_toggle_btn = ttk.Button(toolbar, text="Toggle Log", command=self.toggle_logging, width=12)
        self.log_toggle_btn.pack(side=tk.LEFT, padx=3)

        self.status_msg_label = ttk.Label(toolbar, text="", foreground="gray",
                                           font=self.small_font, width=30)
        self.status_msg_label.pack(side=tk.LEFT, padx=10)
        self.status_msg_timeout = None

        # Main content area
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Left side: IMU Data and Plot
        left_frame = ttk.LabelFrame(main_frame, text="IMU Data & Monitoring", padding="8")
        left_frame.grid(row=0, column=0, sticky="nsew", padx=3)
        main_frame.columnconfigure(0, weight=2)
        main_frame.rowconfigure(0, weight=1)

        imu_grid = ttk.Frame(left_frame)
        imu_grid.pack(fill=tk.X, pady=3)

        ttk.Label(imu_grid, text="Roll:",    font=self.medium_font, width=8).grid(row=0, column=0, sticky=tk.W, padx=2)
        self.roll_label  = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.roll_label.grid(row=0, column=1, sticky=tk.W, padx=5)

        ttk.Label(imu_grid, text="Pitch:",   font=self.medium_font, width=8).grid(row=0, column=2, sticky=tk.W, padx=2)
        self.pitch_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.pitch_label.grid(row=0, column=3, sticky=tk.W, padx=5)

        ttk.Label(imu_grid, text="Yaw:",     font=self.medium_font, width=8).grid(row=1, column=0, sticky=tk.W, padx=2)
        self.yaw_label   = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.yaw_label.grid(row=1, column=1, sticky=tk.W, padx=5)

        ttk.Label(imu_grid, text="Target:",  font=self.medium_font, width=8).grid(row=1, column=2, sticky=tk.W, padx=2)
        self.setpoint_info_label = ttk.Label(imu_grid, text="--", font=self.medium_font,
                                              width=12, anchor=tk.W, foreground="blue")
        self.setpoint_info_label.grid(row=1, column=3, sticky=tk.W, padx=5)

        ttk.Label(imu_grid, text="Balance:", font=self.medium_font, width=8).grid(row=2, column=0, sticky=tk.W, padx=2)
        self.balance_label = ttk.Label(imu_grid, text="--", font=self.large_font, width=12, anchor=tk.W)
        self.balance_label.grid(row=2, column=1, sticky=tk.W, padx=5)

        comm_frame = ttk.Frame(left_frame)
        comm_frame.pack(fill=tk.X, pady=3)
        self.imu_comm_label  = ttk.Label(comm_frame, text="IMU: -- Hz (--%)", font=self.medium_font)
        self.imu_comm_label.pack(side=tk.LEFT, padx=10)
        self.vesc_comm_label = ttk.Label(comm_frame, text="VESC: -- Hz (--%)", font=self.medium_font)
        self.vesc_comm_label.pack(side=tk.LEFT, padx=10)

        self.setup_plot(left_frame)

        # Right side: Tuning Parameters
        right_frame = ttk.LabelFrame(main_frame, text="Tuning Parameters", padding="8")
        right_frame.grid(row=0, column=1, sticky="nsew", padx=3)
        main_frame.columnconfigure(1, weight=1)

        self.setup_tuning_controls(right_frame)

        # Bottom action bar
        action_frame = ttk.Frame(self.root, padding="5")
        action_frame.pack(fill=tk.X)

        ttk.Button(action_frame, text="Show All Values",
                   command=self.show_tuning_values, width=15).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Sync Params",
                   command=self.sync_params, width=12).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Save Settings",
                   command=lambda: self.serial_reader.send_command('k'), width=15).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Load Settings",
                   command=lambda: self.serial_reader.send_command('g'), width=15).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Toggle Stream",
                   command=lambda: self.serial_reader.send_command(' '), width=15).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Open Logs",
                   command=self.open_log_folder, width=15).pack(side=tk.LEFT, padx=3)
        ttk.Button(action_frame, text="Help",
                   command=self.show_help, width=12).pack(side=tk.LEFT, padx=3)

    def setup_plot(self, parent):
        self.fig, self.ax = plt.subplots(figsize=(7, 5), dpi=80)
        self.ax.set_xlabel("Time (s)", fontsize=10)
        self.ax.set_ylabel("Value", fontsize=10)
        self.ax.set_title("Real-time Data", fontsize=12, fontweight='bold')
        self.ax.grid(True, alpha=0.3)

        self.line_roll,    = self.ax.plot([], [], label="Roll (°)",    color='blue',  linewidth=2)
        self.line_pitch,   = self.ax.plot([], [], label="Pitch (°)",   color='green', linewidth=2)
        self.line_current, = self.ax.plot([], [], label="Current (A)", color='red',   linewidth=2)
        self.ax.legend(fontsize=9, loc='upper right')

        self.canvas = FigureCanvasTkAgg(self.fig, parent)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, pady=3)

    def setup_tuning_controls(self, parent):
        self.param_labels = {}
        self.last_param_values = {}
        self.fine_adjust_enabled = tk.BooleanVar(value=False)
        self.fine_adjust_state = False

        # Single-Loop PID
        pid_frame = ttk.LabelFrame(parent, text="PID Control", padding="6")
        pid_frame.pack(fill=tk.X, pady=3)

        self.create_param_control(pid_frame, "Kp", 'p', 'P', 'Kp', 0.5)
        self.create_param_control(pid_frame, "Ki", 'i', 'I', 'Ki', 0.05)
        self.create_param_control(pid_frame, "Kd", 'j', 'D', 'Kd', 0.05)

        fine_frame = ttk.Frame(pid_frame)
        fine_frame.pack(fill=tk.X, pady=2)
        ttk.Label(fine_frame, text="Fine Adjust:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)
        ttk.Checkbutton(fine_frame, text="Enable", variable=self.fine_adjust_enabled,
                        command=self.toggle_fine_adjust).pack(side=tk.LEFT, padx=2)

        diag_frame = ttk.Frame(pid_frame)
        diag_frame.pack(fill=tk.X, pady=2)
        ttk.Label(diag_frame, text="Mode:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)
        self.mode_label = ttk.Label(diag_frame, text="PID", font=self.medium_font, width=12, anchor=tk.CENTER)
        self.mode_label.pack(side=tk.LEFT, padx=5)
        tk.Button(diag_frame, text="Toggle (d)", width=12, font=self.medium_font,
                  command=lambda: self.serial_reader.send_command('d')).pack(side=tk.LEFT, padx=2)

        # Yaw PID
        yaw_frame = ttk.LabelFrame(parent, text="Yaw PID Control (Rotation)", padding="6")
        yaw_frame.pack(fill=tk.X, pady=3)

        self.create_param_control(yaw_frame, "Yaw Kp", 'y', 'Y', 'Kp_yaw', 0.5)
        self.create_param_control(yaw_frame, "Yaw Ki", 'u', 'U', 'Ki_yaw', 0.05)
        self.create_param_control(yaw_frame, "Yaw Kd", 'h', 'H', 'Kd_yaw', 0.05)

        yaw_toggle_frame = ttk.Frame(yaw_frame)
        yaw_toggle_frame.pack(fill=tk.X, pady=2)
        ttk.Label(yaw_toggle_frame, text="Yaw Control:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)
        self.yaw_control_label = ttk.Label(yaw_toggle_frame, text="ENABLED", font=self.medium_font,
                                            width=12, anchor=tk.CENTER)
        self.yaw_control_label.pack(side=tk.LEFT, padx=5)
        tk.Button(yaw_toggle_frame, text="Toggle (n)", width=12, font=self.medium_font,
                  command=lambda: self.serial_reader.send_command('n')).pack(side=tk.LEFT, padx=2)

        # Velocity Control
        velocity_frame = ttk.LabelFrame(parent, text="Velocity Control (Phase 1)", padding="6")
        velocity_frame.pack(fill=tk.X, pady=3)

        self.create_param_control(velocity_frame, "Velocity Setpoint (m/s)", 'v', 'V', 'velocity_setpoint', 0.1)

        vel_display_frame = ttk.Frame(velocity_frame)
        vel_display_frame.pack(fill=tk.X, pady=2)
        ttk.Label(vel_display_frame, text="Current Velocity:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)
        self.velocity_display_label = ttk.Label(vel_display_frame, text="0.00 m/s",
                                                 font=self.medium_font, width=12, anchor=tk.CENTER)
        self.velocity_display_label.pack(side=tk.LEFT, padx=5)
        tk.Button(vel_display_frame, text="Stop (0)", width=12, font=self.medium_font,
                  command=lambda: self.serial_reader.send_command('0')).pack(side=tk.LEFT, padx=2)

        self.create_param_control(velocity_frame, "Vel Kp", 'w', 'W', 'Kp_vel', 0.05)
        self.create_param_control(velocity_frame, "Vel Ki", 'e', 'E', 'Ki_vel', 0.01)

        vel_kd_frame = ttk.Frame(velocity_frame)
        vel_kd_frame.pack(fill=tk.X, pady=2)
        ttk.Label(vel_kd_frame, text="Vel Kd:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)
        vel_kd_label = ttk.Label(vel_kd_frame, text="0.00 (PI only)", font=self.medium_font,
                                  width=12, anchor=tk.CENTER)
        vel_kd_label.pack(side=tk.LEFT, padx=5)
        self.param_labels['Kd_vel'] = vel_kd_label

        # Motor Settings
        motor_frame = ttk.LabelFrame(parent, text="Motor Settings", padding="6")
        motor_frame.pack(fill=tk.X, pady=3)

        self.create_param_control(motor_frame, "Max Current (A)",    'm', 'M', 'max_current',    0.5)
        self.create_param_control(motor_frame, "Angle Setpoint (°)", 'z', 'Z', 'angle_setpoint', 0.1)

    def create_param_control(self, parent, label, dec_cmd, inc_cmd, param_key, step):
        frame = ttk.Frame(parent)
        frame.pack(fill=tk.X, pady=3)

        ttk.Label(frame, text=f"{label}:", font=self.medium_font, width=16).pack(side=tk.LEFT, padx=2)

        value_label = ttk.Label(frame, text="--", font=self.large_font, width=10, anchor=tk.CENTER)
        value_label.pack(side=tk.LEFT, padx=5)
        self.param_labels[param_key] = value_label

        if dec_cmd:
            tk.Button(frame, text="▼", width=4, font=self.medium_font,
                      command=lambda: self.adjust_param(dec_cmd, param_key, -step)).pack(side=tk.LEFT, padx=2)
        else:
            ttk.Label(frame, text="", width=4).pack(side=tk.LEFT, padx=2)

        if inc_cmd:
            tk.Button(frame, text="▲", width=4, font=self.medium_font,
                      command=lambda: self.adjust_param(inc_cmd, param_key, step)).pack(side=tk.LEFT, padx=2)

    # ------------------------------------------------------------------
    # Control actions
    # ------------------------------------------------------------------

    def adjust_param(self, cmd, param_key, step):
        if self.serial_reader.connected:
            self.serial_reader.send_command(cmd)
            param_name = param_key.replace('_', ' ').title()
            direction = "increased" if step > 0 else "decreased"
            self.show_status_message(f"{param_name} {direction}...", duration=2000)

    def toggle_fine_adjust(self):
        if self.serial_reader.connected:
            self.serial_reader.send_command('t')
            self.fine_adjust_state = not self.fine_adjust_state
            status = "ON" if self.fine_adjust_state else "OFF"
            self.show_status_message(f"Fine Adjust {status}", duration=1500, color="blue")

    def refresh_devices(self):
        devices = self.serial_reader.find_devices()
        self.device_info = getattr(self.serial_reader, 'device_info', {})

        display_values = [self.device_info.get(d, d) for d in devices]
        self.device_combo['values'] = display_values
        self.device_map = {disp: dev for dev, disp in zip(devices, display_values)}

        if devices:
            self.device_combo.current(0)
            self._on_device_selected()

    def _on_device_selected(self, event=None):
        pass

    def toggle_connection(self):
        if self.serial_reader.connected:
            self.serial_reader.disconnect()
            self.connect_btn.config(text="Connect")
            self.status_label.config(text="Disconnected", foreground="red")
            self.log_status_label.config(text="Logging: OFF", foreground="gray")
        else:
            display_name = self.device_var.get()
            if not display_name:
                messagebox.showerror("Error", "Please select a device")
                return

            device = self.device_map.get(display_name)
            if not device:
                device = display_name.split(" - ")[-1] if " - " in display_name else display_name

            if self.serial_reader.connect(device):
                self.connect_btn.config(text="Disconnect")
                self.status_label.config(text="Connected", foreground="green")
                if self.serial_reader.log_enabled and self.serial_reader.log_file:
                    log_file = os.path.basename(self.serial_reader.get_log_filename())
                    self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green")
                else:
                    self.log_status_label.config(text="Logging: OFF", foreground="gray")
                self.serial_reader.send_command('x')
                time.sleep(0.1)
                self.serial_reader.send_command(' ')
                time.sleep(0.1)
                self.serial_reader.send_command(' ')

    def toggle_logging(self):
        enabled = self.serial_reader.toggle_logging()
        if enabled and self.serial_reader.connected:
            log_file = os.path.basename(self.serial_reader.get_log_filename())
            self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green")
        else:
            self.log_status_label.config(text="Logging: OFF", foreground="gray")

    def open_log_folder(self):
        if not os.path.exists(self.log_dir):
            messagebox.showinfo("No Logs",
                                "Logs folder doesn't exist yet. Connect to robot to start logging.")
            return
        try:
            if platform.system() == "Windows":
                os.startfile(self.log_dir)
            elif platform.system() == "Darwin":
                subprocess.run(["open", self.log_dir])
            else:
                subprocess.run(["xdg-open", self.log_dir])
        except Exception as e:
            messagebox.showerror("Error",
                                 f"Could not open log folder:\n{e}\n\nLogs are in: {self.log_dir}")

    def show_tuning_values(self):
        if self.serial_reader.connected:
            self.serial_reader.send_command('x')
        else:
            messagebox.showinfo("Not Connected", "Please connect to robot first")

    def sync_params(self):
        if self.serial_reader.connected:
            self.serial_reader.request_sync()
            self.show_status_message("Syncing parameters from firmware...", duration=2000, color="blue")
        else:
            messagebox.showinfo("Not Connected", "Please connect to robot first")

    def show_help(self):
        help_text = """Keyboard Shortcuts (also work in GUI):

Angle PID (Balance):
  p/P - Decrease/Increase Kp
  i/I - Decrease/Increase Ki
  j/D - Decrease/Increase Kd (NOTE: 'd' toggles diagnostic mode)

Velocity Control (Phase 1 Cascaded):
  v/V - Decrease/Increase Velocity Setpoint (m/s)
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
  x/X - Show all tuning values
  @ - Sync GUI with firmware parameters
  SPACE - Pause/Resume data stream
  l/L - Start logging
  s/S - Stop logging
"""
        messagebox.showinfo("Help", help_text)

    # ------------------------------------------------------------------
    # Display update loop
    # ------------------------------------------------------------------

    def start_updates(self):
        self.update_display()
        self.update_job = self.root.after(UPDATE_INTERVAL_MS, self.start_updates)

    def update_display(self):
        data = self.serial_reader.get_data()

        # Connection status
        if data['connected']:
            time_since_update = time.time() - data['last_update']
            if data['last_update'] > 0 and time_since_update > 1.0:
                self.status_label.config(text="Connected (No Data)", foreground="orange",
                                          font=self.medium_font)
            else:
                self.status_label.config(text="Connected", foreground="green", font=self.medium_font)
            if self.serial_reader.log_enabled and self.serial_reader.log_file:
                log_file = os.path.basename(self.serial_reader.get_log_filename())
                self.log_status_label.config(text=f"Logging: ON ({log_file})", foreground="green",
                                              font=self.medium_font)
            else:
                self.log_status_label.config(text="Logging: OFF", foreground="gray", font=self.medium_font)
        else:
            self.status_label.config(text="Disconnected", foreground="red", font=self.medium_font)
            self.log_status_label.config(text="Logging: OFF", foreground="gray", font=self.medium_font)

        # IMU data
        imu = data['imu']
        self.roll_label.config(text=f"{imu['roll']:.2f}°",   font=self.large_font)
        self.pitch_label.config(text=f"{imu['pitch']:.2f}°", font=self.large_font)
        self.yaw_label.config(text=f"{imu['yaw']:.2f}°",     font=self.large_font)

        balance_color = "green" if imu['balance_status'] == 'OK' else "red"
        self.balance_label.config(text=imu['balance_status'], foreground=balance_color,
                                   font=self.large_font)

        # Setpoint / error info
        tuning = data['tuning']
        base_setpoint = tuning.get('angle_setpoint', 0.0)
        drive_offset  = tuning.get('drive_offset', 0.0)
        active_setpoint = base_setpoint + drive_offset
        error = imu['roll'] - active_setpoint
        error_color = "green" if abs(error) < 0.5 else "orange" if abs(error) < 1.0 else "red"
        self.setpoint_info_label.config(
            text=f"{active_setpoint:.1f}° (err: {error:+.2f}°)",
            foreground=error_color, font=self.medium_font,
        )

        # Velocity display
        if hasattr(self, 'velocity_display_label'):
            vel_act = imu.get('velocity_actual', 0.0)
            vel_set = tuning.get('velocity_setpoint', 0.0)
            diff = abs(vel_act - vel_set)
            vel_color = "green" if diff < 0.05 else "orange" if diff < 0.15 else "red"
            self.velocity_display_label.config(text=f"{vel_act:.2f} m/s",
                                                foreground=vel_color, font=self.medium_font)

        # Drive mode indicator
        if hasattr(self, 'drive_mode_label'):
            if abs(drive_offset) < 0.01:
                self.drive_mode_label.config(text="STOP",    foreground="gray",   font=self.medium_font)
            elif drive_offset > 0:
                self.drive_mode_label.config(text="FORWARD", foreground="green",  font=self.medium_font)
            else:
                self.drive_mode_label.config(text="BACK",    foreground="orange", font=self.medium_font)

        # Comm stats
        comm = data['comm']
        if comm['imu_hz'] > 0:
            self.imu_comm_label.config(text=f"IMU: {comm['imu_hz']:.0f} Hz ({comm['imu_rate']:.1f}%)",
                                        font=self.medium_font)
        else:
            self.imu_comm_label.config(text="IMU: -- Hz (--%)", font=self.medium_font)

        if comm['vesc_hz'] > 0:
            self.vesc_comm_label.config(text=f"VESC: {comm['vesc_hz']:.0f} Hz ({comm['vesc_rate']:.1f}%)",
                                         font=self.medium_font)
        else:
            self.vesc_comm_label.config(text="VESC: -- Hz (--%)", font=self.medium_font)

        # Tuning parameter labels
        for key, label in self.param_labels.items():
            value = tuning.get(key, 0)
            old_value = self.last_param_values.get(key)

            if key == 'Kd_vel':
                formatted = "0.00 (PI only)"
            elif key in ('Kp_angle', 'Ki_angle', 'Kd_angle',
                         'Kp_vel',   'Ki_vel',   'Kd_vel',
                         'Kp',       'Ki',       'Kd',
                         'Kp_yaw',   'Ki_yaw',   'Kd_yaw'):
                formatted = f"{value:.2f}"
            elif key in ('max_current', 'min_current'):
                formatted = f"{value:.1f}A"
            elif key in ('angle_setpoint', 'drive_offset'):
                formatted = f"{value:.1f}°"
            elif key == 'deadband':
                formatted = f"{value:.2f}°"
            else:
                formatted = f"{value:.3f}"

            label.config(text=formatted, font=self.large_font)

            if old_value is not None and abs(value - old_value) > 0.001:
                label.config(background="yellow", foreground="black")
                self.root.after(500, lambda l=label: l.config(background="", foreground="black"))
                param_name = key.replace('_', ' ').title()
                self.show_status_message(f"✓ {param_name} updated: {formatted}",
                                          duration=2000, color="green")

            self.last_param_values[key] = value

        # Yaw control label
        if hasattr(self, 'yaw_control_label'):
            yaw_enabled = tuning.get('yaw_control_enabled', True)
            self.yaw_control_label.config(
                text="ENABLED" if yaw_enabled else "DISABLED",
                foreground="green" if yaw_enabled else "red",
            )

        # Control-direction labels
        if hasattr(self, 'roll_sign_label'):
            self.roll_sign_label.config(
                text="INVERTED" if data.get('roll_sign_inverted') else "NORMAL",
                font=self.medium_font,
            )
        if hasattr(self, 'motor_dir_label'):
            self.motor_dir_label.config(
                text="SWAPPED" if data.get('motor_directions_swapped') else "ORIGINAL",
                font=self.medium_font,
            )

        # Real-time plot (uses 'history' key from TeensyComms.get_data)
        hist = data['history']
        if hist['time'] and len(hist['time']) > 0:
            self.line_roll.set_data(hist['time'],    hist['roll'])
            self.line_pitch.set_data(hist['time'],   hist['pitch'])
            self.line_current.set_data(hist['time'], hist['current'])

            t_max = max(hist['time'])
            self.ax.set_xlim(max(0, t_max - 10), t_max + 1)

            all_vals = list(hist['roll']) + list(hist['pitch']) + list(hist['current'])
            if all_vals:
                self.ax.set_ylim(min(all_vals) - 2, max(all_vals) + 2)

            self.canvas.draw()

    def show_status_message(self, message, duration=2000, color="gray"):
        if self.status_msg_timeout:
            self.root.after_cancel(self.status_msg_timeout)
        self.status_msg_label.config(text=message, foreground=color)
        self.status_msg_timeout = self.root.after(
            duration, lambda: self.status_msg_label.config(text="", foreground="gray")
        )


def main():
    root = tk.Tk()
    app = RobotTuningGUI(root)

    def on_closing():
        if app.serial_reader.connected:
            app.serial_reader.disconnect()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
