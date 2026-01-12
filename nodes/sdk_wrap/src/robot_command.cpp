#include <memory>
#include <string>
#include <chrono>
#include <fstream>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nlohmann/json.hpp"

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using namespace limxsdk;
using json = nlohmann::json;

class RobotCommandNode : public rclcpp::Node {
public:
  RobotCommandNode() : Node("robot_command") {
    // Load robot IP from config file
    std::string robot_ip = loadRobotIpFromConfig();
    if (robot_ip.empty()) {
      RCLCPP_ERROR(this->get_logger(), "Failed to load robot IP from config file");
      exit(1);
    }

    // Connect to robot
    pf_ = PointFoot::getInstance();
    if (!pf_->init(robot_ip.c_str())) {
      RCLCPP_ERROR(this->get_logger(), "Failed to init LimX PointFoot with IP %s", robot_ip.c_str());
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
  std::string loadRobotIpFromConfig() {
    try {
      std::ifstream config_file("/root/limx_ws/src/livox_ros_driver2/config/MID360_config.json");
      if (!config_file.is_open()) {
        RCLCPP_WARN(this->get_logger(), "Could not open config file");
        return "";
      }
      json config = json::parse(config_file);
      return config["MID360"]["host_net_info"]["cmd_data_ip"].get<std::string>();
    } catch (const std::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "Error parsing config file: %s", e.what());
      return "";
    }
  }

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
