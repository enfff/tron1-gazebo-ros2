#!/usr/bin/env python3

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
import os

def generate_launch_description():
    # Declare launch arguments
    lidar_type_arg = DeclareLaunchArgument(
        'lidar_type',
        default_value='MID360',
        description='Type of Livox LiDAR (HAP or MID360)'
    )
    
    rviz_arg = DeclareLaunchArgument(
        'rviz',
        default_value='true',
        description='Launch RViz for visualization'
    )
    
    config_file_arg = DeclareLaunchArgument(
        'config_file',
        default_value='',
        description='Path to Livox configuration file (auto-selected if empty)'
    )

    # Get launch configurations
    lidar_type = LaunchConfiguration('lidar_type')
    rviz_enable = LaunchConfiguration('rviz')
    config_file = LaunchConfiguration('config_file')

    # Get package path
    package_path = FindPackageShare('sdk_wrap')

    # Auto-select config file based on lidar type if not specified
    config_file_path = PathJoinSubstitution([
        package_path,
        'config',
        [lidar_type, '_config.json']
    ])

    # Livox ROS Driver 2 node (assuming it's installed)
    livox_driver_node = Node(
        package='livox_ros_driver2',
        executable='livox_ros_driver2_node',
        name='livox_driver',
        parameters=[{
            'xfer_format': 0,  # Livox pointcloud2 format
            'multi_topic': 0,  # All devices use same topic
            'data_src': 0,     # Real LiDAR
            'publish_freq': 10.0,
            'output_data_type': 0,
            'frame_id': 'livox_frame',
            'user_config_path': config_file_path,
            'cmdline_str': '100000000000000',
            'cmdline_file_path': 'livox_test.lvx',
            'enable_lidar_bag': True,
            'enable_imu_bag': True
        }],
        output='screen'
    )

    # TRON1 Livox lidar node
    tron1_livox_node = Node(
        package='sdk_wrap',
        executable='livox_lidar_node',
        name='tron1_livox_lidar',
        parameters=[{
            'frame_id': 'livox_frame',
            'base_frame_id': 'base_link',
            'publish_tf': True,
            'lidar_x_offset': 0.0,
            'lidar_y_offset': 0.0,
            'lidar_z_offset': 0.15,  # 15cm above base_link
            'lidar_roll': 0.0,
            'lidar_pitch': 0.0,
            'lidar_yaw': 0.0
        }],
        output='screen'
    )

    # RViz node
    rviz_config_path = PathJoinSubstitution([
        package_path,
        'config',
        'tron1_livox.rviz'
    ])
    
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config_path],
        condition=IfCondition(rviz_enable),
        output='screen'
    )

    return LaunchDescription([
        lidar_type_arg,
        rviz_arg,
        config_file_arg,
        livox_driver_node,
        tron1_livox_node,
        rviz_node
    ])