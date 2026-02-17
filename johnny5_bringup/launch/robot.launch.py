#!/usr/bin/env python3
"""Master launch file for the Johnny 5 robot (real hardware).

Brings up:
  1. Robot description (URDF / TF tree)
  2. Sensor fusion (EKF)
  3. LiDAR driver
  4. SLAM (optional, default off)
  5. Nav2 (optional, default off)
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_description = get_package_share_directory('johnny5_description')
    pkg_sensor_fusion = get_package_share_directory('johnny5_sensor_fusion')
    pkg_bringup = get_package_share_directory('johnny5_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time')
    enable_slam = LaunchConfiguration('enable_slam')
    enable_nav2 = LaunchConfiguration('enable_nav2')
    use_rviz = LaunchConfiguration('use_rviz')

    return LaunchDescription([
        # --- Arguments ---
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('enable_slam', default_value='false',
                              description='Launch SLAM Toolbox'),
        DeclareLaunchArgument('enable_nav2', default_value='false',
                              description='Launch Nav2 navigation stack'),
        DeclareLaunchArgument('use_rviz', default_value='false',
                              description='Launch RViz'),

        # --- 1. Robot description (URDF + TF) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_description, 'launch', 'robot_description.launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
                'use_rviz': use_rviz,
            }.items(),
        ),

        # --- 2. Sensor fusion (EKF) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_sensor_fusion, 'launch', 'sensor_fusion.launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
            }.items(),
        ),

        # --- 3. LiDAR ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_bringup, 'launch', 'sensors.launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
            }.items(),
        ),

        # --- 4. SLAM Toolbox (optional) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_bringup, 'launch', 'slam.launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
            }.items(),
            condition=IfCondition(enable_slam),
        ),

        # --- 5. Nav2 (optional) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_bringup, 'launch', 'navigation.launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
            }.items(),
            condition=IfCondition(enable_nav2),
        ),
    ])
