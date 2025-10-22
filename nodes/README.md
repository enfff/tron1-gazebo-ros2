# TRON 1 SDK Wrapper

> ⚠️ Before compiling and running the nodes, make sure to enable the odometry and IMU data as explained in the [upper level development](https://support.limxdynamics.com/en/docs/tron-1-sdk/upper-level-development). 

This package wraps the sensor data provided by the SDK into ROS 2 publisher nodes. The code has been written and tested for ROS 2 Jazzy.



| data | ros2 message type | topic |
| --- | --- | --- | 
| IMU (orientation, angular_velocity, linear_acceleration) | [sensor_msgs/msg/imu.msg](https://docs.ros2.org/foxy/api/sensor_msgs/msg/Imu.html) | `/imu` |
| Odometry (pose, twist) | [nav_msgs/msg/Odometry.msg](https://docs.ros2.org/foxy/api/nav_msgs/msg/Odometry.html) | `/odom` |
|  |  |  |


## How to Run

    ros2 run sdk_wrap imu_publisher
    ros2 run sdk_wrap odom_publisher