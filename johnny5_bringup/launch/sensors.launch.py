#!/usr/bin/env python3
"""Launch hardware sensor drivers.

Currently includes:
  - LDROBOT LiDAR (STL-19P / LD19)

Future additions:
  - OAK-D Pro (DepthAI ROS2)
  - ReSpeaker microphone array
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),

        # LDROBOT LiDAR driver
        # In simulation, the Gazebo lidar plugin publishes /scan directly,
        # so this node is only needed on real hardware.
        Node(
            package='ldlidar_stl_ros2',
            executable='ldlidar_stl_ros2_node',
            name='ldlidar_node',
            parameters=[{
                'product_name': 'LDLiDAR_LD19',
                'port_name': '/dev/ldlidar',
                'port_baudrate': 230400,
                'topic_name': 'scan',
                'frame_id': 'laser',
                'laser_scan_dir': False,
                'enable_angle_crop_func': False,
                'use_sim_time': use_sim_time,
            }],
            output='screen',
        ),
    ])
