#!/usr/bin/env python3

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    
    # Launch arguments
    enable_livox_arg = DeclareLaunchArgument(
        'enable_livox',
        default_value='true',
        description='Enable Livox LiDAR'
    )
    
    lidar_type_arg = DeclareLaunchArgument(
        'lidar_type',
        default_value='MID360',
        description='Livox LiDAR type (MID360 or HAP)'
    )

    # Get package directories
    sdk_wrap_share = FindPackageShare('sdk_wrap')
    robot_description_share = FindPackageShare('robot_description')
    
    # URDF file path
    urdf_file = PathJoinSubstitution([
        robot_description_share,
        'pointfoot', 'urdf', 'pointfoot.urdf'
    ])

    # TRON1 SDK nodes
    imu_node = Node(
        package='sdk_wrap',
        executable='imu_publisher',
        name='imu_publisher',
        output='screen'
    )

    odom_node = Node(
        package='sdk_wrap',
        executable='odom_publisher',
        name='odom_publisher',
        output='screen'
    )

    joint_state_node = Node(
        package='sdk_wrap',
        executable='joint_state_publisher',
        name='joint_state_publisher',
        output='screen'
    )

    robot_command_node = Node(
        package='sdk_wrap',
        executable='robot_command',
        name='robot_command',
        output='screen'
    )

    # Robot state publisher
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': urdf_file,
            'use_sim_time': False
        }]
    )

    # Livox LiDAR launch
    livox_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('livox_ros_driver2'),
                'launch',
                'msg_MID360_launch.py'
            ])
        ]),
        condition=IfCondition(LaunchConfiguration('enable_livox'))
    )

    return LaunchDescription([
        enable_livox_arg,
        lidar_type_arg,
        imu_node,
        odom_node,
        joint_state_node,
        robot_command_node,
        robot_state_publisher,
        livox_launch
    ])