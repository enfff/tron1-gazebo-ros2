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
        FindPackageShare('sdk_wrap'),
        'urdf', 'robot.urdf'
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

    # robot_command now handles both WebSocket control AND joint state publishing
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
            'robot_description': Command(['cat ', urdf_file_path]),
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

    # Static transform: livox_frame relative to base_Link  
    # Trying yaw=180° only to flip X direction
    static_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_tf_base_to_livox',
        arguments=[
            '0', '0', '0.2',          # x y z translation
            '0', '0', '3.14159',      # roll=0 pitch=0 yaw=180°
            'base_Link', 'livox_frame'
        ]
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
        remappings=[
            ('cloud_in', '/cloud_in'),      # Input: Converted point cloud (RELIABLE QoS)
            ('scan', '/scan')               # Output: 2D laser scan
        ],
        parameters=[{
            'target_frame': 'base_Link',  # Transform to base_Link for Nav2
            'transform_tolerance': 0.5,
            'min_height': -0.6,              # 1 meter below base_Link
            'max_height': 0.3,               # 0.3 meters above base_Link
            'angle_min': -3.14159,          # -180 degrees
            'angle_max': 3.14159,           # +180 degrees
            'angle_increment': 0.00873,      # ~
            'scan_time': 0.1,               # Scan time for velocity calculations
            'range_min': 0.1,               # Minimum range
            'range_max': 100.0,             # Maximum range
            'use_inf': True,                # Use infinity for max range
            'inf_epsilon': 1.0,             # Epsilon for infinity comparison
            'qos_overrides./scan.publisher.reliability': 'reliable',
            'qos_overrides./scan.publisher.durability': 'volatile',
            'qos_overrides./scan.publisher.history': 'keep_last',
            'qos_overrides./scan.publisher.depth': 10
        }]
    )
    
    # Robot Localization EKF - fuses odom_raw + IMU -> filtered /odom + odom->base_Link TF
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[PathJoinSubstitution([
            FindPackageShare('sdk_wrap'),
            'config', 'ekf_params.yaml'
        ])]
    )
    zed_to_map_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_tf_map_to_zed',
        arguments=[
            '2.433', '3.373', '0',
            '0.0194', '-0.7069', '-0.7069', '0.0194',  # Added 90° rotation around X
            'map', 'zed_frame'
        ]
    )
    return LaunchDescription([
        imu_node,
        odom_node,
        ekf_node,  # EKF for smooth filtered odometry
        # joint_state_node,  # DISABLED: joint states now published by robot_command node
        robot_command_node,  # Now publishes both commands and joint states
        robot_state_publisher,
        livox_launch,
        qos_converter,
        pointcloud_to_laserscan,
        static_tf,  # Added comma here
        # fast_lio_launch
        zed_to_map_tf
    ])