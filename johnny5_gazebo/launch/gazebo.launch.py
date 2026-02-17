#!/usr/bin/env python3
"""Launch Gazebo simulation with the Johnny 5 robot.

Starts:
  1. Gazebo server + client with the indoor test world
  2. Robot description (URDF with Gazebo plugins)
  3. Robot spawner
  4. Sensor fusion (EKF)
  5. Optionally: SLAM and/or Nav2
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_gazebo = get_package_share_directory('johnny5_gazebo')
    pkg_description = get_package_share_directory('johnny5_description')
    pkg_sensor_fusion = get_package_share_directory('johnny5_sensor_fusion')
    pkg_bringup = get_package_share_directory('johnny5_bringup')

    urdf_file = os.path.join(pkg_description, 'urdf', 'johnny5.urdf.xacro')
    world_file = os.path.join(pkg_gazebo, 'worlds', 'johnny5_world.world')

    use_rviz = LaunchConfiguration('use_rviz')
    enable_slam = LaunchConfiguration('enable_slam')
    enable_nav2 = LaunchConfiguration('enable_nav2')

    robot_description = Command(['xacro ', urdf_file, ' use_sim:=true'])

    return LaunchDescription([
        # --- Arguments ---
        DeclareLaunchArgument('use_rviz', default_value='true',
                              description='Launch RViz'),
        DeclareLaunchArgument('enable_slam', default_value='true',
                              description='Launch SLAM Toolbox'),
        DeclareLaunchArgument('enable_nav2', default_value='false',
                              description='Launch Nav2 navigation stack'),

        # --- 1. Gazebo ---
        ExecuteProcess(
            cmd=['gazebo', '--verbose', world_file,
                 '-s', 'libgazebo_ros_init.so',
                 '-s', 'libgazebo_ros_factory.so'],
            output='screen',
        ),

        # --- 2. Robot description ---
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'robot_description': robot_description,
                'use_sim_time': True,
            }],
        ),

        # --- 3. Spawn robot in Gazebo ---
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='spawn_johnny5',
            arguments=[
                '-topic', 'robot_description',
                '-entity', 'johnny5',
                '-x', '0.0',
                '-y', '0.0',
                '-z', '0.02',  # just above ground; base_footprint is ground-plane
            ],
            output='screen',
        ),

        # --- 4. Sensor fusion (EKF) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_sensor_fusion, 'launch', 'sensor_fusion.launch.py')
            ),
            launch_arguments={'use_sim_time': 'true'}.items(),
        ),

        # --- 5. SLAM (optional, default on for sim) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_bringup, 'launch', 'slam.launch.py')
            ),
            launch_arguments={'use_sim_time': 'true'}.items(),
            condition=IfCondition(enable_slam),
        ),

        # --- 6. Nav2 (optional) ---
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_bringup, 'launch', 'navigation.launch.py')
            ),
            launch_arguments={'use_sim_time': 'true'}.items(),
            condition=IfCondition(enable_nav2),
        ),

        # --- 7. RViz ---
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', os.path.join(pkg_description, 'rviz', 'johnny5.rviz')],
            condition=IfCondition(use_rviz),
            parameters=[{'use_sim_time': True}],
        ),
    ])
