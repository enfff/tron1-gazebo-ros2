#include <memory>
#include <string>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using std::placeholders::_1;
using namespace std::chrono_literals;

class JointStatePublisherNode : public rclcpp::Node {
public:
  JointStatePublisherNode() : Node("joint_state_publisher") {
    publisher_ = this->create_publisher<sensor_msgs::msg::JointState>("/joint_states", rclcpp::SensorDataQoS());

    // Initialize LimX SDK and subscribe to robot state using the fixed robot IP
    auto pf = limxsdk::PointFoot::getInstance();
    constexpr const char* kRobotIp = "10.192.1.2";
    if (!pf->init(kRobotIp)) {
      RCLCPP_ERROR(this->get_logger(), "Failed to init LimX PointFoot with IP %s", kRobotIp);
      // We don't throw; allow retries or later connection
    } else {
      RCLCPP_INFO(this->get_logger(), "Connected to robot at %s", kRobotIp);
    }

    // Subscribe to robot state updates and publish into ROS 2
    limxsdk::PointFoot::getInstance()->subscribeRobotState(
      [this](const limxsdk::RobotStateConstPtr &state) {
        publishJointState(*state);
      }
    );

    RCLCPP_INFO(this->get_logger(), "Joint state publisher ready. Publishing to /joint_states");
  }

private:
  void publishJointState(const limxsdk::RobotState &state) {
    sensor_msgs::msg::JointState msg;

    // Timestamp in nanoseconds from SDK
    msg.header.stamp.sec = static_cast<int32_t>(state.stamp / 1000000000ULL);
    msg.header.stamp.nanosec = static_cast<uint32_t>(state.stamp % 1000000000ULL);

    // Resize vectors to match the number of joints
    size_t num_joints = state.q.size();
    msg.name.resize(num_joints);
    msg.position.resize(num_joints);
    msg.velocity.resize(num_joints);
    msg.effort.resize(num_joints);

    // Fill joint data
    for (size_t i = 0; i < num_joints; ++i) {
      // Generate joint names (you may want to customize these based on your robot)
      msg.name[i] = "joint_" + std::to_string(i);
      msg.position[i] = state.q[i];     // Joint angles in radians
      msg.velocity[i] = state.dq[i];    // Joint velocities in rad/s
      msg.effort[i] = state.tau[i];     // Joint torques in N·m
    }

    publisher_->publish(msg);
  }

  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr publisher_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<JointStatePublisherNode>();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}