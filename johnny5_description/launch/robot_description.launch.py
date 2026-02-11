#!/usr/bin/env python3
"""Launch robot_state_publisher with the Johnny 5 URDF."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_description = get_package_share_directory('johnny5_description')

    urdf_file = os.path.join(pkg_description, 'urdf', 'johnny5.urdf.xacro')

    use_sim_time = LaunchConfiguration('use_sim_time')
    use_rviz = LaunchConfiguration('use_rviz')

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time', default_value='false',
            description='Use simulation clock'),

        DeclareLaunchArgument(
            'use_rviz', default_value='false',
            description='Launch RViz for visualization'),

        # robot_state_publisher: publishes /robot_description and TF tree
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'robot_description': Command(['xacro ', urdf_file]),
                'use_sim_time': use_sim_time,
            }],
        ),

        # joint_state_publisher: publishes dummy joint states for fixed joints
        # (wheels are driven by diff_drive in sim or by FSESC on hardware)
        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            name='joint_state_publisher',
            parameters=[{'use_sim_time': use_sim_time}],
        ),

        # RViz (optional)
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', os.path.join(pkg_description, 'rviz', 'johnny5.rviz')],
            condition=IfCondition(use_rviz),
            parameters=[{'use_sim_time': use_sim_time}],
        ),
    ])
