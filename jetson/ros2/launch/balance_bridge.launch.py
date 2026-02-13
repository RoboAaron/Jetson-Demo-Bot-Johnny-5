"""
Launch file for the balance_bridge ROS 2 package.

Starts:
  balance_bridge_node  — Teensy ↔ ROS 2 bridge (/odom, /imu/*, /robot_state)
  esp32_joy_node       — ESP32 PS3 bridge → /joy
  teleop_twist_joy     — /joy → /cmd_vel_joy
  twist_mux            — merges /cmd_vel_joy, /cmd_vel_nav, /cmd_vel_web,
                         /cmd_vel_voice → /cmd_vel  (priority: joy > nav > web > voice)

Usage:
    ros2 launch balance_bridge balance_bridge.launch.py
    ros2 launch balance_bridge balance_bridge.launch.py device:=/dev/ttyACM0
    ros2 launch balance_bridge balance_bridge.launch.py \\
        device:=/dev/ttyACM0 esp32_device:=/dev/ttyUSB0 debug:=true

Environment variable BALANCE_BRIDGE_REPO_ROOT (optional):
    If set, PYTHONPATH is extended to reach jetson_bridge and teensy_comms
    when running from a colcon install (not needed when running from source).
"""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('balance_bridge')
    twist_mux_cfg      = os.path.join(pkg_share, 'config', 'twist_mux.yaml')
    teleop_twist_cfg   = os.path.join(pkg_share, 'config', 'teleop_twist_joy.yaml')

    # Extend PYTHONPATH for colcon installs if BALANCE_BRIDGE_REPO_ROOT is set
    repo_root = os.environ.get('BALANCE_BRIDGE_REPO_ROOT', '')
    extra_python = []
    if repo_root:
        extra_python = [
            os.path.join(repo_root, 'jetson'),
            os.path.join(repo_root, 'tuning_code'),
        ]
    new_pythonpath = os.pathsep.join(
        filter(None, extra_python + [os.environ.get('PYTHONPATH', '')])
    )

    return LaunchDescription([
        # ── Launch arguments ─────────────────────────────────────────────
        DeclareLaunchArgument('device',          default_value='',
            description='Teensy serial device (empty = auto-detect)'),
        DeclareLaunchArgument('watchdog_s',      default_value='0.5',
            description='Seconds before safety halt on missing /cmd_vel'),
        DeclareLaunchArgument('publish_rate_hz', default_value='20.0',
            description='State topic publication rate (Hz)'),
        DeclareLaunchArgument('debug',           default_value='false',
            description='Enable TeensyComms debug logging'),
        DeclareLaunchArgument('esp32_device',    default_value='',
            description='ESP32 serial device (empty = auto-detect /dev/ttyUSB*)'),

        # ── PYTHONPATH ───────────────────────────────────────────────────
        SetEnvironmentVariable('PYTHONPATH', new_pythonpath),

        # ── balance_bridge_node ──────────────────────────────────────────
        # Publishes: /odom, /imu/roll|pitch|yaw, /balance/vel, /robot_state
        # Subscribes: /cmd_vel  (from twist_mux output)
        Node(
            package='balance_bridge',
            executable='balance_bridge_node',
            name='balance_bridge_node',
            output='screen',
            parameters=[{
                'device':          LaunchConfiguration('device'),
                'watchdog_s':      LaunchConfiguration('watchdog_s'),
                'publish_rate_hz': LaunchConfiguration('publish_rate_hz'),
                'debug':           LaunchConfiguration('debug'),
            }],
        ),

        # ── esp32_joy_node ───────────────────────────────────────────────
        # Reads JSON from ESP32 over USB serial, publishes sensor_msgs/Joy on /joy
        Node(
            package='balance_bridge',
            executable='esp32_joy_node',
            name='esp32_joy_node',
            output='screen',
            parameters=[{
                'device': LaunchConfiguration('esp32_device'),
            }],
        ),

        # ── teleop_twist_joy ─────────────────────────────────────────────
        # Converts /joy → /cmd_vel_joy (R1 deadman, left-stick fwd, right-stick turn)
        Node(
            package='teleop_twist_joy',
            executable='teleop_node',
            name='teleop_twist_joy',
            output='screen',
            parameters=[teleop_twist_cfg],
            remappings=[('cmd_vel', '/cmd_vel_joy')],
        ),

        # ── twist_mux ────────────────────────────────────────────────────
        # Merges /cmd_vel_joy (100) > /cmd_vel_nav (50) > /cmd_vel_web (25)
        #        > /cmd_vel_voice (10)  →  /cmd_vel
        Node(
            package='twist_mux',
            executable='twist_mux',
            name='twist_mux',
            output='screen',
            parameters=[twist_mux_cfg],
            remappings=[('cmd_vel_out', '/cmd_vel')],
        ),
    ])
