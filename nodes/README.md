# TRON 1 SDK Wrapper

> ⚠️ Before compiling and running the nodes, make sure to enable the odometry and IMU data as explained in the [upper level development](https://support.limxdynamics.com/en/docs/tron-1-sdk/upper-level-development). 

This package wraps the sensor data provided by the SDK into ROS 2 publisher nodes, and provides a launch file to easily run all the sensors including the Livox MID360 Lidar. The code has been written and tested for ROS 2 Jazzy.

The table below summarizes all the information about the nodes involved

| data | ros2 message type | topic |
| --- | --- | --- | 
| IMU (orientation, angular_velocity, linear_acceleration) | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/imu` |
| Odometry (pose, twist) | [nav_msgs/msg/Odometry.msg](https://docs.ros2.org/foxy/api/nav_msgs/msg/Odometry.html) | `/odom` |
| Joint States (position, velocity, effort) | [sensor_msgs/msg/JointState.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/JointState.html) | `/joint_states` |
| Robot Commands (velocity commands) | [geometry_msgs/msg/Twist.msg](https://docs.ros2.org/foxy/api/geometry_msgs/msg/Twist.html) | `/cmd_vel` |
| LiDAR Point Cloud (custom format) | [livox_ros_driver2/msg/CustomMsg](src/livox_ros_driver2/msg/CustomMsg.msg) | `/livox/points` |
| LiDAR IMU | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/livox/imu` |


## TODOs

- [x] Launching the livox node requires a custom configuration and lunch file. Find a way to sync them after building
<details>
<summary>Fix frame_id field for livox node</summary>

```
[livox_lidar_publisher]: Data Source is raw lidar.
[livox_ros_driver2_node-1] [INFO] [1763381643.007388426] [livox_lidar_publisher]: Config file: /root/limx_ws/src/livox_ros_driver2/launch/../config/MID360_config.json
[livox_ros_driver2_node-1] LdsLidar *GetInstance
[livox_ros_driver2_node-1] config lidar type: 8
[livox_ros_driver2_node-1] successfully parse base config, counts: 1
[livox_ros_driver2_node-1] [INFO] [1763381643.009903931] [livox_lidar_publisher]: Init lds lidar success!
[livox_ros_driver2_node-1] GetFreeIndex key:livox_lidar_84000778.
[livox_ros_driver2_node-1] set pcl data type, handle: 84000778, data type: 1
[livox_ros_driver2_node-1] set scan pattern, handle: 84000778, scan pattern: 0
[livox_ros_driver2_node-1] begin to change work mode to 'Normal', handle: 84000778
[livox_ros_driver2_node-1] successfully set data type, handle: 84000778, set_bit: 2
[livox_ros_driver2_node-1] successfully set pattern mode, handle: 84000778, set_bit: 0
[livox_ros_driver2_node-1] successfully set lidar attitude, ip: 10.192.1.5
[livox_ros_driver2_node-1] successfully change work mode, handle: 84000778
[livox_ros_driver2_node-1] successfully enable Livox Lidar imu, ip: 10.192.1.5
[livox_ros_driver2_node-1] [INFO] [1763381651.171796190] [livox_lidar_publisher]: livox/imu publish use imu format
[livox_ros_driver2_node-1] [INFO] [1763381651.280314094] [livox_lidar_publisher]: livox/points publish use PointCloud2 format
[livox_ros_driver2_node-1] Init queue, real query size:16.
[livox_ros_driver2_node-1] Lidar[0] storage queue size: 10
^C[WARNING] [launch]: user interrupted with ctrl-c (SIGINT)
[livox_ros_driver2_node-1] [INFO] [1763381670.237243503] [rclcpp]: signal_handler(signum=2)
[livox_ros_driver2_node-1] Livox Lidar SDK Deinit completely!
[livox_ros_driver2_node-1] lddc destory!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
[livox_ros_driver2_node-1] lds destory!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
[INFO] [livox_ros_driver2_node-1]: process has finished cleanly [pid 31992]
```
</details>

- [x] Buffer node to retransmit lidar data w/ QOS "best effort" to the converter node
- [ ] Update Dockerfile to replace the lidar configuration file (msg_MID360_config.json) *after* the driver installation


## How to Run

Before launching nodes:
1. configure the `MID360_config.json` in `src/livox_ros_driver2/config/MID360_config.json with` the correct IP addresses (see [IP Addresses](#ip-addresses))
2. Set the variable `xfer_format` in `src/livox_ros_driver2/launch/msg_MID360_launch.py` to `0`

A working configuration can be found [here](sdk_wrap/config/msg_MID360_config.json)

**Single launch file**

    ros2 launch sdk_wrap sensors_launch.py

The lidar tf can be configured in this launch file

**Individual nodes**

    ros2 run sdk_wrap imu_publisher
    ros2 run sdk_wrap odom_publisher
    ros2 run sdk_wrap joint_state_publisher
    ros2 run sdk_wrap robot_command
    ros2 launch livox_ros_driver2 msg_MID360_launch.py

# IP Addresses

Addresses vary with your network layout. For a reliable setup, configure your host (laptop or Jetson) with a LAN profile using:

- IP address: `10.192.1.120`
- Netmask: `255.255.255.0`
- Default gateway: `10.192.1.2`

On the ethernet switch¹, connect: 
- Port 1: robot
- Port 2: LiDAR
- Port 3: host

This wiring guarantees proper communication between all devices.

¹Note: this code has been tested using a SWITCH TP-LINK LS108G ethernet switch

<!-- 10.192.1.2 -->

| Component | IP Address |
|-:|:-|
|TRON 1 (SDK)|10.192.1.2|
|Host IP|10.192.1.120|
|LiDAR|10.192.1.121|

Where XX are the last two digits of the lidar serial number 

## SLAM and Navigation

### SLAM Toolbox

Launch SLAM Toolbox with custom parameters:

```bash
ros2 launch slam_toolbox online_async_launch.py slam_params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/slam_params.yaml
```

Customize the `slam_params.yaml` file in `sdk_wrap/config/` to adjust SLAM parameters such as mapping resolution, loop closure detection, and scan matching tolerances.

### Nav2 (Navigation 2)

Launch Nav2 with custom parameters:

```bash
ros2 launch nav2_bringup navigation_launch.py params_file:=/home/jack92/Documents/tron1-gazebo-ros2/nodes/sdk_wrap/config/nav2_params.yaml
```

Adjust the `nav2_params.yaml` file in `sdk_wrap/config/` to configure navigation parameters such as planner settings, controller gains, and costmap resolution. 
