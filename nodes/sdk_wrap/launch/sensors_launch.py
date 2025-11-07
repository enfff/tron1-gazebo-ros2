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

    # Livox LiDAR
    livox_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('livox_ros_driver2'),
                'launch',
                'msg_MID360_launch.py'
            ])
        ]),
    )
    
    # Fast-LIO mapping launch
    fast_lio_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('fast_lio'),
                'launch',
                'mapping.launch.py'
            ])
        ]),
        launch_arguments={'config_file': 'mid360.yaml'}.items()
    )
    
    # QoS Converter: BEST_EFFORT -> RELIABLE
    qos_converter = Node(
        package='sdk_wrap',
        executable='qos_converter_node',
        name='qos_converter_node',
        output='screen'
    )

    # PointCloud to LaserScan converter (now uses /cloud_in with RELIABLE QoS)
    pointcloud_to_laserscan = Node(
        package='pointcloud_to_laserscan',
        executable='pointcloud_to_laserscan_node',
        name='pointcloud_to_laserscan',
        # remappings=[
        #     ('cloud_in', '/cloud_in'),      # Input: Converted point cloud (RELIABLE QoS)
        #     ('scan', '/laserscan')          # Output: 2D laser scan
        # ],
        parameters=[{
            'transform_tolerance': 0.01,
            'min_height': -0.5,             # Lower bound for Z-axis filtering
            'max_height': 2.0,              # Upper bound for Z-axis filtering  
            'angle_min': -3.14159,          # -180 degrees
            'angle_max': 3.14159,           # +180 degrees
            'angle_increment': 0.0087,      # ~0.5 degrees resolution
            'scan_time': 0.1,               # Scan time for velocity calculations
            'range_min': 0.1,               # Minimum range
            'range_max': 100.0,             # Maximum range
            'use_inf': True,                # Use infinity for max range
            'inf_epsilon': 1.0              # Epsilon for infinity comparison
        }]
    )
    
    

    return LaunchDescription([
        imu_node,
        odom_node,
        joint_state_node,
        robot_command_node,
        robot_state_publisher,
        livox_launch,
        qos_converter,
        pointcloud_to_laserscan,
        # fast_lio_launch
    ])