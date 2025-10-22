#include <memory>
#include <string>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using std::placeholders::_1;
using namespace std::chrono_literals;

class ImuPublisherNode : public rclcpp::Node {
public:
  ImuPublisherNode() : Node("imu_publisher") {
    publisher_ = this->create_publisher<sensor_msgs::msg::Imu>("/imu", rclcpp::SensorDataQoS());

    // Initialize LimX SDK and subscribe to IMU using the fixed robot IP
    auto pf = limxsdk::PointFoot::getInstance();
    constexpr const char* kRobotIp = "10.192.1.2";
    if (!pf->init(kRobotIp)) {
      RCLCPP_ERROR(this->get_logger(), "Failed to init LimX PointFoot with IP %s", kRobotIp);
      // We don't throw; allow retries or later connection
    } else {
      RCLCPP_INFO(this->get_logger(), "Connected to robot at %s", kRobotIp);
    }

    // Subscribe to IMU updates and publish into ROS 2
    limxsdk::PointFoot::getInstance()->subscribeImuData(
      [this](const limxsdk::ImuDataConstPtr &imu) {
        publishImu(*imu);
      }
    );

    RCLCPP_INFO(this->get_logger(), "IMU publisher ready. Publishing to /imu");
  }

private:
  void publishImu(const limxsdk::ImuData &imu) {
    sensor_msgs::msg::Imu msg;

    // Timestamp in nanoseconds from SDK
    msg.header.frame_id = "imu"; // fixed IMU frame id
    msg.header.stamp.sec = static_cast<int32_t>(imu.stamp / 1000000000ULL);
    msg.header.stamp.nanosec = static_cast<uint32_t>(imu.stamp % 1000000000ULL);

    // Orientation: SDK quat order is (w, x, y, z); ROS expects x,y,z,w
    msg.orientation.x = imu.quat[1];
    msg.orientation.y = imu.quat[2];
    msg.orientation.z = imu.quat[3];
    msg.orientation.w = imu.quat[0];

    // Angular velocity (rad/s)
    msg.angular_velocity.x = imu.gyro[0];
    msg.angular_velocity.y = imu.gyro[1];
    msg.angular_velocity.z = imu.gyro[2];

    // Linear acceleration (m/s^2)
    msg.linear_acceleration.x = imu.acc[0];
    msg.linear_acceleration.y = imu.acc[1];
    msg.linear_acceleration.z = imu.acc[2];

    // Covariance unknown per ROS convention
    msg.orientation_covariance[0] = -1.0;
    msg.angular_velocity_covariance[0] = -1.0;
    msg.linear_acceleration_covariance[0] = -1.0;

    publisher_->publish(msg);
  }
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr publisher_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ImuPublisherNode>();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
