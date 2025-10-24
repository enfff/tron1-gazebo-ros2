#include <memory>
#include <string>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using namespace limxsdk;

class RobotCommandNode : public rclcpp::Node {
public:
  RobotCommandNode() : Node("robot_command") {
    // Connect to robot
    pf_ = PointFoot::getInstance();
    constexpr const char* kRobotIp = "10.192.1.2";
    if (!pf_->init(kRobotIp)) {
      RCLCPP_ERROR(this->get_logger(), "Failed to init LimX PointFoot with IP %s", kRobotIp);
      exit(1);
    }
    motor_num_ = pf_->getMotorNumber();
    cmd_ = std::make_shared<RobotCmd>(motor_num_);
    // Default command: zero velocity in velocity mode
    for (size_t i = 0; i < cmd_->mode.size(); ++i) {
      cmd_->mode[i] = 1; // velocity mode
      cmd_->dq[i] = 0.0;
    }
    cmd_->stamp = now_nanoseconds();
    pf_->publishRobotCmd(*cmd_);

    // Subscribe to cmd_vel
    sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", rclcpp::QoS(10),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        handle_twist(*msg);
      }
    );

    RCLCPP_INFO(this->get_logger(), "robot_command node ready. Listening to /cmd_vel");
  }

  ~RobotCommandNode() = default;

private:
  static uint64_t now_nanoseconds() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  void handle_twist(const geometry_msgs::msg::Twist& twist) {
    // Update command vectors with the incoming twist; adjust mapping as needed per robot specification.
    for (size_t i = 0; i < cmd_->dq.size(); ++i) {
      cmd_->mode[i] = 1; // 1 = velocity mode
      cmd_->dq[i] = twist.linear.x;
    }
    cmd_->stamp = now_nanoseconds();
    pf_->publishRobotCmd(*cmd_);
  }

  PointFoot* pf_;
  uint32_t motor_num_;
  std::shared_ptr<RobotCmd> cmd_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RobotCommandNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
