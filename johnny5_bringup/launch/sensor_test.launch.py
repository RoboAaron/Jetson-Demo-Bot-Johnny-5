"""
sensor_test.launch.py — Minimal sensor-only launch for integration testing.

Starts only the requested sensors. No SLAM, no Nav2, no EKF.
Use this to validate that all sensors publish correctly before layering
navigation on top.

Usage:
    # Bridge + LiDAR (default)
    ros2 launch johnny5_bringup sensor_test.launch.py

    # Bridge + LiDAR + OAK-D
    ros2 launch johnny5_bringup sensor_test.launch.py enable_oakd:=true

    # LiDAR only (no bridge)
    ros2 launch johnny5_bringup sensor_test.launch.py enable_bridge:=false

    # Everything
    ros2 launch johnny5_bringup sensor_test.launch.py \\
        enable_bridge:=true enable_lidar:=true enable_oakd:=true enable_static_tf:=true

Arguments:
    enable_bridge   (bool, default true)  — balance_bridge_node (/odom, /imu/*)
    enable_lidar    (bool, default true)  — ldlidar driver (/scan)
    enable_oakd     (bool, default false) — depthai_ros_driver (/oak/*)
    enable_static_tf (bool, default true) — base_link → laser static TF

Note: This does NOT start twist_mux, esp32_joy_node, or teleop_twist_joy.
      If you need those, use balance_bridge.launch.py instead.
"""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def generate_launch_description():

    return LaunchDescription([
        # ── Arguments ────────────────────────────────────────────────────
        DeclareLaunchArgument('enable_bridge', default_value='true',
            description='Start balance_bridge_node (Teensy bridge)'),
        DeclareLaunchArgument('enable_lidar', default_value='true',
            description='Start LDROBOT LiDAR driver'),
        DeclareLaunchArgument('enable_oakd', default_value='false',
            description='Start OAK-D depthai_ros_driver'),
        DeclareLaunchArgument('enable_static_tf', default_value='true',
            description='Publish base_link → laser static TF'),

        DeclareLaunchArgument('teensy_device', default_value='',
            description='Teensy serial device (empty = auto-detect)'),
        DeclareLaunchArgument('lidar_port', default_value='/dev/ldlidar',
            description='LiDAR serial port'),

        # Laser TF offsets (metres, relative to base_link)
        DeclareLaunchArgument('laser_x', default_value='0.0',
            description='Laser X offset from base_link (metres, forward)'),
        DeclareLaunchArgument('laser_y', default_value='0.0',
            description='Laser Y offset from base_link (metres, left)'),
        DeclareLaunchArgument('laser_z', default_value='0.15',
            description='Laser Z offset from base_link (metres, up)'),

        # ── balance_bridge_node ──────────────────────────────────────────
        # Lightweight: just the bridge node, no twist_mux or joy
        Node(
            condition=IfCondition(LaunchConfiguration('enable_bridge')),
            package='balance_bridge',
            executable='balance_bridge_node',
            name='balance_bridge_node',
            output='screen',
            parameters=[{
                'device':          LaunchConfiguration('teensy_device'),
                'watchdog_s':      0.5,
                'publish_rate_hz': 20.0,
                'debug':           False,
            }],
        ),

        # ── LDROBOT LiDAR ────────────────────────────────────────────────
        Node(
            condition=IfCondition(LaunchConfiguration('enable_lidar')),
            package='ldlidar_stl_ros2',
            executable='ldlidar_stl_ros2_node',
            name='ldlidar_node',
            output='screen',
            parameters=[{
                'product_name':  'LDLiDAR_LD19',
                'topic_name':    '/scan',
                'frame_id':      'laser',
                'port_name':     LaunchConfiguration('lidar_port'),
                'port_baudrate': 230400,
            }],
        ),

        # ── Static TF: base_link → laser ────────────────────────────────
        Node(
            condition=IfCondition(LaunchConfiguration('enable_static_tf')),
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_to_laser',
            arguments=[
                LaunchConfiguration('laser_x'),
                LaunchConfiguration('laser_y'),
                LaunchConfiguration('laser_z'),
                '0', '0', '0',    # roll, pitch, yaw (radians)
                'base_link',
                'laser',
            ],
        ),

        # ── OAK-D depthai_ros_driver ─────────────────────────────────────
        # Conditional — only if enable_oakd:=true
        # Uses default depthai_ros_driver config.  Override with params file
        # if you need custom resolution / FPS / depth settings.
        Node(
            condition=IfCondition(LaunchConfiguration('enable_oakd')),
            package='depthai_ros_driver',
            executable='camera_node',
            name='oakd_camera',
            output='screen',
        ),
    ])
