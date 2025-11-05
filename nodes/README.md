# TRON 1 SDK Wrapper

> ⚠️ Before compiling and running the nodes, make sure to enable the odometry and IMU data as explained in the [upper level development](https://support.limxdynamics.com/en/docs/tron-1-sdk/upper-level-development). 

This package wraps the sensor data provided by the SDK into ROS 2 publisher nodes. The code has been written and tested for ROS 2 Jazzy.

| data | ros2 message type | topic |
| --- | --- | --- | 
| IMU (orientation, angular_velocity, linear_acceleration) | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/imu` |
| Odometry (pose, twist) | [nav_msgs/msg/Odometry.msg](https://docs.ros2.org/foxy/api/nav_msgs/msg/Odometry.html) | `/odom` |
| Joint States (position, velocity, effort) | [sensor_msgs/msg/JointState.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/JointState.html) | `/joint_states` |
| Robot Commands (velocity commands) | [geometry_msgs/msg/Twist.msg](https://docs.ros2.org/foxy/api/geometry_msgs/msg/Twist.html) | `/cmd_vel` |
| LiDAR Point Cloud (custom format) | [livox_ros_driver2/msg/CustomMsg](src/livox_ros_driver2/msg/CustomMsg.msg) | `/livox/points` |
| LiDAR IMU | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/livox/imu` |


## How to Run

**Individual nodes**

    ros2 run sdk_wrap imu_publisher
    ros2 run sdk_wrap odom_publisher
    ros2 run sdk_wrap joint_state_publisher
    ros2 run sdk_wrap robot_command
    ros2 launch livox_ros_driver2 msg_MID360_launch.py

<!-- ros2 run sdk_wrap livox_lidar_node -->

**Single launch file**

    ros2 launch sdk_wrap sensors_launch.py

# Area

| Component | IP Address |
|:-:|:-:|
|TRON 1 (SDK)|10.192.1.2|
|LiDAR|10.192.1.5|