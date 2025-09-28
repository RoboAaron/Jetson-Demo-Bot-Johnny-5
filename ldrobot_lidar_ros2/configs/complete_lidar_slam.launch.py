#!/usr/bin/env python3

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # Declare launch arguments
    port_arg = DeclareLaunchArgument(
        'port_name',
        default_value='/dev/ldlidar',
        description='Serial port for LiDAR'
    )
    
    baudrate_arg = DeclareLaunchArgument(
        'port_baudrate',
        default_value='230400',
        description='Baud rate for LiDAR'
    )
    
    # LiDAR node
    lidar_node = Node(
        package='ldlidar_stl_ros2',
        executable='ldlidar_stl_ros2_node',
        name='ldlidar_node',
        parameters=[{
            'product_name': 'LDLiDAR_LD19',
            'port_name': LaunchConfiguration('port_name'),
            'port_baudrate': LaunchConfiguration('port_baudrate'),
            'topic_name': 'scan',
            'frame_id': 'laser',
            'laser_scan_dir': False,
            'enable_angle_crop_func': False
        }],
        output='screen'
    )
    
    # TF Chain: map -> odom -> base_link -> laser
    tf_map_odom = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='map_to_odom_tf',
        arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom'],
        output='screen'
    )
    
    tf_odom_base = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='odom_to_base_tf',
        arguments=['0', '0', '0', '0', '0', '0', 'odom', 'base_link'],
        output='screen'
    )
    
    tf_base_laser = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='base_to_laser_tf',
        arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'laser'],
        output='screen'
    )
    
    # SLAM Toolbox node
    slam_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        parameters=[{
            'use_sim_time': False,
            'base_frame': 'base_link',
            'odom_frame': 'odom',
            'map_frame': 'map',
            'scan_topic': '/scan',
            'mode': 'mapping',
            'transform_publish_period': 0.05,
            'map_update_interval': 0.1,
            'minimum_travel_distance': 0.1,
            'minimum_travel_heading': 0.1
        }],
        output='screen'
    )
    
    return LaunchDescription([
        port_arg,
        baudrate_arg,
        lidar_node,
        tf_map_odom,
        tf_odom_base,
        tf_base_laser,
        slam_node
    ])

