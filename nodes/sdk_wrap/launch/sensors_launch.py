#!/usr/bin/env python3

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution, Command
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
# from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    
    # sdk_wrap_share = FindPackageShare('sdk_wrap')
    
    # Read URDF file content
    urdf_file_path = PathJoinSubstitution([
        FindPackageShare('robot_description'),
        'pointfoot', 'WF_TRON1A', 'urdf', 'robot.urdf'
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
            'robot_description': Command(['xacro ', urdf_file_path]),
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
    )

    return LaunchDescription([
        imu_node,
        odom_node,
        joint_state_node,
        robot_command_node,
        robot_state_publisher,
        livox_launch
    ])