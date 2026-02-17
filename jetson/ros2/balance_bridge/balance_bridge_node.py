#!/usr/bin/env python3
"""
ROS 2 node: balance_bridge_node

Bridges the Teensy balance controller to ROS 2.

Subscriptions:
  /cmd_vel  (geometry_msgs/Twist)   — Nav2 / teleop velocity commands
                                      linear.x  → vel_mps
                                      angular.z → steer_rads

Publications:
  /robot_state  (std_msgs/String)       — JSON-encoded snapshot of TeensyComms state
                                          (20 Hz, matches Teensy telemetry rate)
  /imu/roll     (std_msgs/Float32)      — roll angle in degrees
  /imu/pitch    (std_msgs/Float32)      — pitch angle in degrees
  /imu/yaw      (std_msgs/Float32)      — yaw angle in degrees
  /balance/vel  (std_msgs/Float32)      — measured forward velocity in m/s
  /odom         (nav_msgs/Odometry)     — dead-reckoning odometry for Nav2
                                          integrated from encoder velocity + IMU yaw

Odometry frame convention:
  frame_id      = "odom"
  child_frame_id = "base_link"
  pose   integrated from Vel: (encoder m/s) and Y: (IMU yaw degrees)
  twist  populated with instantaneous Vel and yaw-rate derived from IMU

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
import math
import sys
import os
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist, TransformStamped
from nav_msgs.msg import Odometry
from std_msgs.msg import String, Float32
from tf2_ros import TransformBroadcaster

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
        self._state_pub = self.create_publisher(String,   '/robot_state', 10)
        self._roll_pub  = self.create_publisher(Float32,  '/imu/roll',    10)
        self._pitch_pub = self.create_publisher(Float32,  '/imu/pitch',   10)
        self._yaw_pub   = self.create_publisher(Float32,  '/imu/yaw',     10)
        self._vel_pub   = self.create_publisher(Float32,  '/balance/vel', 10)
        self._odom_pub  = self.create_publisher(Odometry, '/odom',        10)

        # ── TF broadcaster (odom → base_link) ────────────────────────────
        self._tf_broadcaster = TransformBroadcaster(self)

        # ── Dead-reckoning state ──────────────────────────────────────────
        self._odom_x     = 0.0   # metres
        self._odom_y     = 0.0
        self._odom_theta = 0.0   # radians
        self._last_yaw_deg   = None   # previous IMU yaw (degrees) for rate estimate
        self._last_odom_time = None   # time.monotonic() of last odom update

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

        self._publish_odom(imu)

        # Compact JSON (omit history buffers to keep message small)
        self._state_pub.publish(String(data=json.dumps({
            'imu':       imu,
            'tuning':    state['tuning'],
            'comm':      state['comm'],
            'connected': state['connected'],
        })))

    # ── Odometry ──────────────────────────────────────────────────────────

    def _publish_odom(self, imu: dict):
        """Integrate encoder velocity + IMU yaw into a nav_msgs/Odometry message.

        Integration method:
          - Linear velocity  v     = imu['velocity_actual']  (m/s, from encoders)
          - Yaw angle        theta = imu['yaw'] converted to radians (from BNO085)
          - yaw_rate         omega = Δyaw / Δt               (rad/s)
          - x += v * cos(theta) * dt
          - y += v * sin(theta) * dt
          - theta is taken directly from the IMU (no integration drift on heading)

        Covariance: diagonal, conservative — tuned for a slow balance robot.
        Nav2 will fuse this with lidar SLAM in Wave 3 (PBI-13).
        """
        now = time.monotonic()
        yaw_deg = float(imu['yaw'])
        vel     = float(imu['velocity_actual'])

        if self._last_odom_time is None:
            # First call — seed state, don't publish yet
            self._last_odom_time = now
            self._last_yaw_deg   = yaw_deg
            self._odom_theta     = math.radians(yaw_deg)
            return

        dt = now - self._last_odom_time
        if dt <= 0.0:
            return

        # Heading directly from IMU (avoids integration drift on theta)
        theta     = math.radians(yaw_deg)
        yaw_rate  = math.radians(yaw_deg - self._last_yaw_deg) / dt

        # Integrate position
        self._odom_x += vel * math.cos(theta) * dt
        self._odom_y += vel * math.sin(theta) * dt
        self._odom_theta      = theta
        self._last_odom_time  = now
        self._last_yaw_deg    = yaw_deg

        # ── Build Odometry message ──────────────────────────────────────
        stamp = self.get_clock().now().to_msg()

        odom = Odometry()
        odom.header.stamp    = stamp
        odom.header.frame_id = 'odom'
        odom.child_frame_id  = 'base_link'

        # Pose
        odom.pose.pose.position.x = self._odom_x
        odom.pose.pose.position.y = self._odom_y
        odom.pose.pose.position.z = 0.0
        # Yaw → quaternion (z-axis rotation only)
        cy, sy = math.cos(theta * 0.5), math.sin(theta * 0.5)
        odom.pose.pose.orientation.x = 0.0
        odom.pose.pose.orientation.y = 0.0
        odom.pose.pose.orientation.z = sy
        odom.pose.pose.orientation.w = cy
        # Pose covariance (row-major 6×6, indices: x=0,y=7,z=14,rx=21,ry=28,rz=35)
        # Conservative diagonal — balance robot odometry drifts more than ground robot
        POSE_COV = [0.0] * 36
        POSE_COV[0]  = 0.05   # x
        POSE_COV[7]  = 0.05   # y
        POSE_COV[14] = 1e9    # z (irrelevant — 2D robot)
        POSE_COV[21] = 1e9    # roll (irrelevant)
        POSE_COV[28] = 1e9    # pitch (irrelevant)
        POSE_COV[35] = 0.02   # yaw
        odom.pose.covariance = POSE_COV

        # Twist (body frame)
        odom.twist.twist.linear.x  = vel
        odom.twist.twist.angular.z = yaw_rate
        TWIST_COV = [0.0] * 36
        TWIST_COV[0]  = 0.01
        TWIST_COV[35] = 0.01
        odom.twist.covariance = TWIST_COV

        self._odom_pub.publish(odom)

        # ── TF: odom → base_link ────────────────────────────────────────
        tf = TransformStamped()
        tf.header.stamp    = stamp
        tf.header.frame_id = 'odom'
        tf.child_frame_id  = 'base_link'
        tf.transform.translation.x = self._odom_x
        tf.transform.translation.y = self._odom_y
        tf.transform.translation.z = 0.0
        tf.transform.rotation.x = 0.0
        tf.transform.rotation.y = 0.0
        tf.transform.rotation.z = sy
        tf.transform.rotation.w = cy
        self._tf_broadcaster.sendTransform(tf)

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
