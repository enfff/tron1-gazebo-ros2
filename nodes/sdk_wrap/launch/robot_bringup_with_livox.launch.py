#!/usr/bin/env python3

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Declare launch arguments
    enable_livox_arg = DeclareLaunchArgument(
        'enable_livox',
        default_value='false',
        description='Enable Livox LiDAR integration'
    )
    
    lidar_type_arg = DeclareLaunchArgument(
        'lidar_type',
        default_value='MID360',
        description='Type of Livox LiDAR (HAP or MID360)'
    )
    
    # Get launch configurations
    enable_livox = LaunchConfiguration('enable_livox')
    lidar_type = LaunchConfiguration('lidar_type')
    
    # Get URDF file path
    urdf_file = '/root/limx_ws/src/robot-description/pointfoot/WF_TRON1A/urdf/robot.urdf'
    
    # Read URDF file
    with open(urdf_file, 'r') as infp:
        robot_description = infp.read()

    # Get package path for launch file inclusion
    package_path = FindPackageShare('sdk_wrap')
    
    # Livox launch file inclusion
    livox_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                package_path,
                'launch',
                'tron1_livox_launch.py'
            ])
        ]),
        launch_arguments={
            'lidar_type': lidar_type,
            'rviz': 'false'  # Disable RViz in the included launch to avoid conflicts
        }.items(),
        condition=IfCondition(enable_livox)
    )

    return LaunchDescription([
        enable_livox_arg,
        lidar_type_arg,
        
        # Robot State Publisher - converts joint_states + URDF -> TF
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'robot_description': robot_description,
                'publish_frequency': 30.0
            }]
        ),
        
        # SDK Wrapper nodes - all sensor data and command interface
        Node(
            package='sdk_wrap',
            executable='imu_publisher',
            name='imu_publisher',
            output='screen'
        ),
        
        Node(
            package='sdk_wrap',
            executable='odom_publisher', 
            name='odom_publisher',
            output='screen'
        ),
        
        Node(
            package='sdk_wrap',
            executable='joint_state_publisher',
            name='joint_state_publisher',
            output='screen'
        ),
        
        Node(
            package='sdk_wrap',
            executable='robot_command',
            name='robot_command',
            output='screen'
        ),
        
        # Include Livox launch if enabled
        livox_launch,
    ])