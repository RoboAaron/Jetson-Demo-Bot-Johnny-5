#!/usr/bin/env python3
"""Launch the EKF node from robot_localization for sensor fusion.

Fuses wheel odometry (from Teensy/FSESC or Gazebo) with BNO085 IMU data
to produce a filtered odometry estimate and odom -> base_link TF.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_sensor_fusion = get_package_share_directory('johnny5_sensor_fusion')

    ekf_config = os.path.join(pkg_sensor_fusion, 'config', 'ekf.yaml')

    use_sim_time = LaunchConfiguration('use_sim_time')

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time', default_value='false',
            description='Use simulation clock'),

        # EKF node: fuses /odom + /imu/data -> /odometry/filtered + TF
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            output='screen',
            parameters=[
                ekf_config,
                {'use_sim_time': use_sim_time},
            ],
            remappings=[
                ('odometry/filtered', '/odometry/filtered'),
            ],
        ),
    ])
