# TRON 1 SDK Wrapper

> ⚠️ Before compiling and running the nodes, make sure to enable the odometry and IMU data as explained in the [upper level development](https://support.limxdynamics.com/en/docs/tron-1-sdk/upper-level-development). 

This package wraps the sensor data provided by the SDK into ROS 2 publisher nodes. The code has been written and tested for ROS 2 Jazzy.



| data | ros2 message type | topic |
| --- | --- | --- | 
| IMU (orientation, angular_velocity, linear_acceleration) | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/imu` |
| Odometry (pose, twist) | [nav_msgs/msg/Odometry.msg](https://docs.ros2.org/foxy/api/nav_msgs/msg/Odometry.html) | `/odom` |
| Joint States (position, velocity, effort) | [sensor_msgs/msg/JointState.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/JointState.html) | `/joint_states` |
|  |  |  |


## How to Run

### Individual nodes:
    ros2 run sdk_wrap imu_publisher
    ros2 run sdk_wrap odom_publisher
    ros2 run sdk_wrap joint_state_publisher

### Complete robot bringup (recommended):
**Launches all nodes: IMU, odometry, joint states, command interface + robot_state_publisher for TF transforms**

    ros2 launch sdk_wrap robot_bringup.launch.py

This single command gives you:
- `/imu` - IMU data for localization
- `/odom` - Odometry data for navigation  
- `/joint_states` - Joint states for visualization
- `/cmd_vel` - Command interface for robot control
- TF transforms for RViz visualization