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
  Watchdog handled by JetsonBridge (sends 0,0 after WATCHDOG_TIMEOUT_S with
  no /cmd_vel message).

Usage:
    ros2 run <package> balance_bridge_node [--ros-args -p device:=/dev/ttyACM0]

Parameters (ROS 2):
  device          string  ''              auto-detect Teensy
  watchdog_s      float   0.5            seconds before safety halt
  publish_rate_hz float   20.0           state publication rate
  debug           bool    false           TeensyComms debug logging
"""

import sys
import os
import json
import threading

import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter
from geometry_msgs.msg import Twist
from std_msgs.msg import String, Float32

# Locate the jetson_bridge module regardless of working directory
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from jetson_bridge import JetsonBridge


class BalanceBridgeNode(Node):

    def __init__(self):
        super().__init__('balance_bridge_node')

        # Declare and read parameters
        self.declare_parameter('device',          '')
        self.declare_parameter('watchdog_s',      0.5)
        self.declare_parameter('publish_rate_hz', 20.0)
        self.declare_parameter('debug',           False)

        device         = self.get_parameter('device').value or None
        watchdog_s     = self.get_parameter('watchdog_s').value
        publish_hz     = self.get_parameter('publish_rate_hz').value
        debug          = self.get_parameter('debug').value

        # Build log directory relative to this file
        log_dir = os.path.join(os.path.dirname(__file__), '..', 'logs')

        # Start bridge
        self._bridge = JetsonBridge(
            device=device,
            watchdog_timeout=watchdog_s,
            log_dir=log_dir,
            debug=debug,
        )
        if not self._bridge.start():
            self.get_logger().error("Failed to connect to Teensy — node will not publish")

        # Subscriptions
        self._cmd_sub = self.create_subscription(
            Twist, '/cmd_vel', self._cmd_vel_cb, 10
        )

        # Publications
        self._state_pub = self.create_publisher(String,  '/robot_state', 10)
        self._roll_pub  = self.create_publisher(Float32, '/imu/roll',    10)
        self._pitch_pub = self.create_publisher(Float32, '/imu/pitch',   10)
        self._yaw_pub   = self.create_publisher(Float32, '/imu/yaw',     10)
        self._vel_pub   = self.create_publisher(Float32, '/balance/vel', 10)

        # Publish timer
        period = 1.0 / publish_hz
        self._timer = self.create_timer(period, self._publish_state)

        self.get_logger().info(
            f"balance_bridge_node started  "
            f"device={device or 'auto'}  "
            f"watchdog={watchdog_s}s  "
            f"rate={publish_hz}Hz"
        )

    # ------------------------------------------------------------------
    # /cmd_vel callback
    # ------------------------------------------------------------------

    def _cmd_vel_cb(self, msg: Twist):
        """Forward Nav2 / teleop velocity to the Teensy."""
        vel_mps    = msg.linear.x
        steer_rads = msg.angular.z
        ok = self._bridge.set_velocity(vel_mps, steer_rads)
        if not ok:
            self.get_logger().warn("set_velocity failed — Teensy not connected?")

    # ------------------------------------------------------------------
    # State publication
    # ------------------------------------------------------------------

    def _publish_state(self):
        state = self._bridge.get_state()
        if not state['connected']:
            return

        imu = state['imu']

        # Individual signal topics
        self._roll_pub.publish(Float32(data=float(imu['roll'])))
        self._pitch_pub.publish(Float32(data=float(imu['pitch'])))
        self._yaw_pub.publish(Float32(data=float(imu['yaw'])))
        self._vel_pub.publish(Float32(data=float(imu['velocity_actual'])))

        # Full JSON state (drop history buffers to keep message small)
        compact = {
            'imu':     imu,
            'tuning':  state['tuning'],
            'comm':    state['comm'],
            'connected': state['connected'],
        }
        self._state_pub.publish(String(data=json.dumps(compact)))

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

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
