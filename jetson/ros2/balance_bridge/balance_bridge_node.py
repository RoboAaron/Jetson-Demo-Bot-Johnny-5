#!/usr/bin/env python3
"""
ROS 2 node: balance_bridge_node

Bridges the Teensy balance controller to ROS 2.

Subscriptions:
  /cmd_vel  (geometry_msgs/Twist)   — Nav2 / teleop velocity commands
                                      linear.x  → vel_mps
                                      angular.z → steer_rads

Publications:
  /robot_state  (std_msgs/String)   — JSON-encoded snapshot of TeensyComms state
                                      (20 Hz, matches Teensy telemetry rate)
  /imu/roll     (std_msgs/Float32)  — roll angle in degrees
  /imu/pitch    (std_msgs/Float32)  — pitch angle in degrees
  /imu/yaw      (std_msgs/Float32)  — yaw angle in degrees
  /balance/vel  (std_msgs/Float32)  — measured forward velocity in m/s

Safety:
  Watchdog handled by JetsonBridge (sends 0,0 after watchdog_s seconds with
  no /cmd_vel message).

Usage:
    ros2 run balance_bridge balance_bridge_node [--ros-args -p device:=/dev/ttyACM0]
    ros2 launch balance_bridge balance_bridge.launch.py

Parameters (ROS 2):
  device          string  ''      auto-detect Teensy
  watchdog_s      float   0.5    seconds before safety halt
  publish_rate_hz float   20.0   state publication rate
  debug           bool    false   TeensyComms debug logging
"""

import json
import sys
import os

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from std_msgs.msg import String, Float32

# Import JetsonBridge.
# The package __init__.py already adds jetson/ and tuning_code/ to sys.path
# when running from the source tree.  If running from a colcon install with
# BALANCE_BRIDGE_REPO_ROOT set, the launch file handles PYTHONPATH.
import balance_bridge  # noqa: F401 — triggers __init__.py sys.path bootstrap
from jetson_bridge import JetsonBridge


class BalanceBridgeNode(Node):

    def __init__(self):
        super().__init__('balance_bridge_node')

        # ── ROS 2 parameters ─────────────────────────────────────────────
        self.declare_parameter('device',          '')
        self.declare_parameter('watchdog_s',      0.5)
        self.declare_parameter('publish_rate_hz', 20.0)
        self.declare_parameter('debug',           False)

        device     = self.get_parameter('device').value or None
        watchdog_s = self.get_parameter('watchdog_s').value
        publish_hz = self.get_parameter('publish_rate_hz').value
        debug      = self.get_parameter('debug').value

        # ── JetsonBridge ─────────────────────────────────────────────────
        log_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'logs')
        self._bridge = JetsonBridge(
            device=device,
            watchdog_timeout=watchdog_s,
            log_dir=log_dir,
            debug=debug,
        )
        if not self._bridge.start():
            self.get_logger().error(
                "Failed to connect to Teensy — topics will be silent until connected"
            )

        # ── Subscriptions ─────────────────────────────────────────────────
        self._cmd_sub = self.create_subscription(
            Twist, '/cmd_vel', self._cmd_vel_cb, 10
        )

        # ── Publications ──────────────────────────────────────────────────
        self._state_pub = self.create_publisher(String,  '/robot_state', 10)
        self._roll_pub  = self.create_publisher(Float32, '/imu/roll',    10)
        self._pitch_pub = self.create_publisher(Float32, '/imu/pitch',   10)
        self._yaw_pub   = self.create_publisher(Float32, '/imu/yaw',     10)
        self._vel_pub   = self.create_publisher(Float32, '/balance/vel', 10)

        # ── Publish timer ─────────────────────────────────────────────────
        self._timer = self.create_timer(1.0 / publish_hz, self._publish_state)

        self.get_logger().info(
            f"balance_bridge_node started  "
            f"device={device or 'auto'}  "
            f"watchdog={watchdog_s}s  "
            f"rate={publish_hz}Hz"
        )

    # ── /cmd_vel callback ─────────────────────────────────────────────────

    def _cmd_vel_cb(self, msg: Twist):
        """Forward Nav2 / teleop velocity to the Teensy."""
        ok = self._bridge.set_velocity(msg.linear.x, msg.angular.z)
        if not ok:
            self.get_logger().warn(
                "set_velocity failed — Teensy not connected?", throttle_duration_sec=5.0
            )

    # ── State publication ─────────────────────────────────────────────────

    def _publish_state(self):
        state = self._bridge.get_state()
        if not state['connected']:
            return

        imu = state['imu']

        self._roll_pub.publish(Float32(data=float(imu['roll'])))
        self._pitch_pub.publish(Float32(data=float(imu['pitch'])))
        self._yaw_pub.publish(Float32(data=float(imu['yaw'])))
        self._vel_pub.publish(Float32(data=float(imu['velocity_actual'])))

        # Compact JSON (omit history buffers to keep message small)
        self._state_pub.publish(String(data=json.dumps({
            'imu':       imu,
            'tuning':    state['tuning'],
            'comm':      state['comm'],
            'connected': state['connected'],
        })))

    # ── Cleanup ───────────────────────────────────────────────────────────

    def destroy_node(self):
        self._bridge.stop()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = BalanceBridgeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
