#!/usr/bin/env python3

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

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
    
    frame_id_arg = DeclareLaunchArgument(
        'frame_id',
        default_value='laser',
        description='Frame ID for laser scan'
    )
    
    topic_name_arg = DeclareLaunchArgument(
        'topic_name',
        default_value='scan',
        description='Topic name for laser scan'
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
            'topic_name': LaunchConfiguration('topic_name'),
            'frame_id': LaunchConfiguration('frame_id'),
            'laser_scan_dir': False,
            'enable_angle_crop_func': False
        }],
        output='screen'
    )
    
    return LaunchDescription([
        port_arg,
        baudrate_arg,
        frame_id_arg,
        topic_name_arg,
        lidar_node
    ])
