#!/usr/bin/env python3
"""
ROS 2 node: esp32_joy_node

Reads newline-delimited JSON from the ESP32 PS3 bridge over USB serial and
publishes sensor_msgs/Joy on /joy.  teleop_twist_joy then converts /joy → /cmd_vel_joy
which enters twist_mux at the highest priority.

JSON format from firmware (esp32/ps3_bridge/ps3_bridge.ino):
  {"lx":<-1..1>,"ly":<-1..1>,"rx":<-1..1>,"ry":<-1..1>,
   "b_x":0|1,..., "connected":0|1}

/joy axis mapping (matches teleop_twist_joy defaults):
  axes[0]  lx  — left stick horizontal
  axes[1]  ly  — left stick vertical    (→ cmd_vel linear.x via teleop_twist_joy)
  axes[2]  rx  — right stick horizontal (→ cmd_vel angular.z, negated by twist_joy config)
  axes[3]  ry  — right stick vertical

/joy button mapping:
  buttons[0]  cross (X)
  buttons[1]  circle (O)
  buttons[2]  square
  buttons[3]  triangle
  buttons[4]  L1
  buttons[5]  R1
  buttons[6]  L2
  buttons[7]  R2
  buttons[8]  PS
  buttons[9]  start
  buttons[10] select

Parameters (ROS 2):
  device      string  ''       auto-detect ESP32 (first /dev/ttyUSB* or /dev/ttyACM*)
  baud        int     115200   serial baud rate
  frame_id    string  'joy'    Joy message frame_id
"""

import glob
import json
import sys
import os
import threading

import serial
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy


_AXIS_KEYS    = ['lx', 'ly', 'rx', 'ry']
_BUTTON_KEYS  = ['b_x', 'b_o', 'b_sq', 'b_tr',
                 'b_l1', 'b_r1', 'b_l2', 'b_r2',
                 'b_ps', 'b_start', 'b_sel']
_BAUD         = 115200


def _auto_detect_esp32() -> str:
    """Return first likely ESP32/Arduino serial device, or empty string."""
    for pattern in ('/dev/ttyUSB*', '/dev/ttyACM*'):
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    return ''


class Esp32JoyNode(Node):

    def __init__(self):
        super().__init__('esp32_joy_node')

        self.declare_parameter('device',   '')
        self.declare_parameter('baud',     _BAUD)
        self.declare_parameter('frame_id', 'joy')

        device   = self.get_parameter('device').value   or _auto_detect_esp32()
        baud     = self.get_parameter('baud').value
        frame_id = self.get_parameter('frame_id').value

        if not device:
            self.get_logger().error('No ESP32 serial device found. '
                                    'Set device param: --ros-args -p device:=/dev/ttyUSB0')
            raise RuntimeError('No serial device')

        self._frame_id  = frame_id
        self._pub       = self.create_publisher(Joy, '/joy', 10)
        self._connected = False

        self.get_logger().info(f'Opening ESP32 serial: {device} @ {baud}')
        self._ser = serial.Serial(device, baud, timeout=1.0)

        # Reader thread — serial is blocking, run outside the ROS executor
        self._running = True
        self._thread  = threading.Thread(target=self._read_loop,
                                         daemon=True, name='esp32-serial')
        self._thread.start()

    # ── Serial reader ──────────────────────────────────────────────────────

    def _read_loop(self):
        buf = b''
        while self._running and rclpy.ok():
            try:
                chunk = self._ser.read(64)
            except serial.SerialException as exc:
                self.get_logger().error(f'Serial read error: {exc}')
                break

            buf += chunk
            while b'\n' in buf:
                line, buf = buf.split(b'\n', 1)
                self._handle_line(line.strip())

    def _handle_line(self, line: bytes):
        if not line or line.startswith(b'#'):
            return  # ignore comments / empty
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            return

        # Connectivity events
        if 'event' in data:
            evt = data['event']
            if evt == 'connected':
                self.get_logger().info('PS3 controller connected')
                self._connected = True
            elif evt == 'disconnected':
                self.get_logger().warn('PS3 controller disconnected')
                self._connected = False
            return

        connected = bool(data.get('connected', 0))
        if connected != self._connected:
            self._connected = connected
            state = 'connected' if connected else 'disconnected'
            self.get_logger().info(f'PS3 controller {state}')

        # Build and publish Joy message
        msg               = Joy()
        msg.header.stamp  = self.get_clock().now().to_msg()
        msg.header.frame_id = self._frame_id
        msg.axes    = [float(data.get(k, 0.0)) for k in _AXIS_KEYS]
        msg.buttons = [int(data.get(k, 0))     for k in _BUTTON_KEYS]
        self._pub.publish(msg)

    # ── Cleanup ────────────────────────────────────────────────────────────

    def destroy_node(self):
        self._running = False
        try:
            self._ser.close()
        except Exception:
            pass
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = Esp32JoyNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
