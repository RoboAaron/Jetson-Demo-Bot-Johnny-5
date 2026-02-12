"""
Launch file for the balance_bridge ROS 2 node.

Usage:
    ros2 launch balance_bridge balance_bridge.launch.py
    ros2 launch balance_bridge balance_bridge.launch.py device:=/dev/ttyACM0
    ros2 launch balance_bridge balance_bridge.launch.py device:=/dev/ttyACM0 debug:=true

Environment variable BALANCE_BRIDGE_REPO_ROOT (optional):
    If set, PYTHONPATH is extended to reach jetson_bridge and teensy_comms.
    Leave unset when running from a colcon-installed package (the __init__.py
    bootstrap handles source-tree installs automatically).
"""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # If the user points at the repo root we extend PYTHONPATH so that
    # jetson_bridge and teensy_comms are importable from a colcon install.
    repo_root = os.environ.get('BALANCE_BRIDGE_REPO_ROOT', '')
    extra_pythonpath_entries = []
    if repo_root:
        extra_pythonpath_entries = [
            os.path.join(repo_root, 'jetson'),
            os.path.join(repo_root, 'tuning_code'),
        ]

    current_pythonpath = os.environ.get('PYTHONPATH', '')
    new_pythonpath = os.pathsep.join(
        filter(None, extra_pythonpath_entries + [current_pythonpath])
    )

    return LaunchDescription([
        # ── launch arguments ────────────────────────────────────────────────
        DeclareLaunchArgument(
            'device', default_value='',
            description='Serial device path (empty = auto-detect Teensy)'
        ),
        DeclareLaunchArgument(
            'watchdog_s', default_value='0.5',
            description='Seconds before safety halt on missing /cmd_vel'
        ),
        DeclareLaunchArgument(
            'publish_rate_hz', default_value='20.0',
            description='State topic publication rate (Hz)'
        ),
        DeclareLaunchArgument(
            'debug', default_value='false',
            description='Enable TeensyComms debug logging'
        ),

        # ── PYTHONPATH (only relevant for colcon-installed + REPO_ROOT set) ─
        SetEnvironmentVariable('PYTHONPATH', new_pythonpath),

        # ── node ─────────────────────────────────────────────────────────────
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
    ])
