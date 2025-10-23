#include <memory>
#include <string>
#include <chrono>
#include <thread>

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

    // Subscribe to cmd_vel
    sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", rclcpp::QoS(10),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        handle_twist(*msg);
      }
    );

    // Start periodic publisher thread
    pub_thread_ = std::thread([this]() {
      while (rclcpp::ok()) {
        pf_->publishRobotCmd(*cmd_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });

    RCLCPP_INFO(this->get_logger(), "robot_command node ready. Listening to /cmd_vel");
  }

  ~RobotCommandNode() {
    if (pub_thread_.joinable()) {
      pub_thread_.join();
    }
  }

private:
  void handle_twist(const geometry_msgs::msg::Twist& twist) {
    RCLCPP_INFO(this->get_logger(), "Received cmd_vel: linear.x=%.2f angular.z=%.2f", twist.linear.x, twist.angular.z);
    for (size_t i = 0; i < cmd_->dq.size(); ++i) {
      cmd_->mode[i] = 1; // 1 = velocity mode
      cmd_->dq[i] = twist.linear.x;
    }
  }

  PointFoot* pf_;
  uint32_t motor_num_;
  std::shared_ptr<RobotCmd> cmd_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
  std::thread pub_thread_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RobotCommandNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
