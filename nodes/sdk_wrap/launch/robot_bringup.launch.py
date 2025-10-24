#!/usr/bin/env python3

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Get URDF file path
    urdf_file = '/root/limx_ws/src/robot-description/pointfoot/WF_TRON1A/urdf/robot.urdf'
    
    # Read URDF file
    with open(urdf_file, 'r') as infp:
        robot_description = infp.read()

    return LaunchDescription([
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
    ])