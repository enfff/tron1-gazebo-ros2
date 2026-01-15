#include <memory>
#include <string>
#include <chrono>
#include <fstream>
#include <atomic>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "nlohmann/json.hpp"

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using namespace limxsdk;
using json = nlohmann::json;

class RobotCommandNode : public rclcpp::Node {
public:
  RobotCommandNode() : Node("robot_command") {
    // Create joint state publisher
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", rclcpp::SensorDataQoS());

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
    
    RCLCPP_INFO(this->get_logger(), "SDK initialized successfully");
    motor_num_ = pf_->getMotorNumber();
    RCLCPP_INFO(this->get_logger(), "Robot has %u motors", motor_num_);
    
    cmd_ = std::make_shared<RobotCmd>(motor_num_);
    // Default command: zero velocity in velocity mode
    for (size_t i = 0; i < cmd_->mode.size(); ++i) {
      cmd_->mode[i] = 1; // velocity mode
      cmd_->dq[i] = 0.0;
    }
    cmd_->stamp = now_nanoseconds();
    pf_->publishRobotCmd(*cmd_);

    // Create a timer to periodically publish joint states
    // Note: We don't resend robot commands here to avoid overriding cmd_vel
    cmd_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(10),  // 100Hz
      [this]() {
        // WORKAROUND: Publish fake joint states since SDK callback isn't working
        // This at least makes RViz visualization work
        publishFakeJointState();
      }
    );

    // Subscribe to robot state and publish as joint states
    RCLCPP_INFO(this->get_logger(), "Subscribing to robot state...");
    pf_->subscribeRobotState(
      [this](const RobotStateConstPtr &state) {
        RCLCPP_INFO_ONCE(this->get_logger(), "First robot state received! Publishing joint states...");
        publishJointState(*state);
      }
    );
    RCLCPP_INFO(this->get_logger(), "Robot state subscription registered");

    // Subscribe to cmd_vel
    sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", rclcpp::QoS(10),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        handle_twist(*msg);
      }
    );

    RCLCPP_INFO(this->get_logger(), "robot_command node ready. Listening to /cmd_vel and publishing /joint_states");
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
    // Convert Twist (linear.x, angular.z) to differential drive wheel velocities
    // Robot has wheel-legged biped configuration with wheels at indices 3 (left) and 7 (right)
    // Differential drive: v_left = linear.x - (angular.z * wheel_base / 2)
    //                     v_right = linear.x + (angular.z * wheel_base / 2)
    
    const double wheel_base = 0.26;  // Distance between wheels in meters (from URDF collision box)
    const double wheel_radius = 0.0625;  // Wheel radius in meters (approximate)
    
    // Calculate wheel linear velocities
    double v_left = twist.linear.x - (twist.angular.z * wheel_base / 2.0);
    double v_right = twist.linear.x + (twist.angular.z * wheel_base / 2.0);
    
    // Convert to wheel angular velocities (rad/s)
    double omega_left = v_left / wheel_radius;
    double omega_right = v_right / wheel_radius;
    
    // Set all motors to position hold mode (0) except wheels
    for (size_t i = 0; i < cmd_->dq.size(); ++i) {
      cmd_->mode[i] = 0;  // Position hold for leg joints
      cmd_->dq[i] = 0.0;
    }
    
    // Set wheel velocities (index 3 = wheel_L, index 7 = wheel_R)
    if (cmd_->mode.size() > 7) {
      cmd_->mode[3] = 1;  // Velocity mode for left wheel
      cmd_->dq[3] = omega_left;
      
      cmd_->mode[7] = 1;  // Velocity mode for right wheel
      cmd_->dq[7] = omega_right;
    }
    
    cmd_->stamp = now_nanoseconds();
    pf_->publishRobotCmd(*cmd_);
    
    // Log commands for debugging
    RCLCPP_DEBUG(this->get_logger(), 
                 "cmd_vel: linear.x=%.3f, angular.z=%.3f -> wheel_L=%.3f, wheel_R=%.3f rad/s",
                 twist.linear.x, twist.angular.z, omega_left, omega_right);
  }

  void publishJointState(const RobotState &state) {
    sensor_msgs::msg::JointState msg;

    // Set frame_id
    msg.header.frame_id = "";
    
    // Timestamp in nanoseconds from SDK
    msg.header.stamp.sec = static_cast<int32_t>(state.stamp / 1000000000ULL);
    msg.header.stamp.nanosec = static_cast<uint32_t>(state.stamp % 1000000000ULL);

    // Joint names for Wheel-foot robot (8 joints)
    // SDK order: 0: abad_L, 1: hip_L, 2: knee_L, 3: wheel_L
    //            4: abad_R, 5: hip_R, 6: knee_R, 7: wheel_R
    static const std::vector<std::string> joint_names = {
      "abad_L_Joint", "hip_L_Joint", "knee_L_Joint", "wheel_L_Joint",
      "abad_R_Joint", "hip_R_Joint", "knee_R_Joint", "wheel_R_Joint"
    };

    // Resize vectors to match the number of joints
    size_t num_joints = state.q.size();
    msg.name.resize(num_joints);
    msg.position.resize(num_joints);
    msg.velocity.resize(num_joints);
    msg.effort.resize(num_joints);

    // Fill joint data
    for (size_t i = 0; i < num_joints; ++i) {
      if (i < joint_names.size()) {
        msg.name[i] = joint_names[i];
      } else {
        msg.name[i] = "joint_" + std::to_string(i);
      }
      msg.position[i] = state.q[i];
      msg.velocity[i] = state.dq[i];
      msg.effort[i] = state.tau[i];
    }

    joint_state_pub_->publish(msg);
  }

  void publishFakeJointState() {
    // TEMPORARY WORKAROUND: Publish zero joint states so RViz can display the robot
    // Replace this when SDK callback starts working
    sensor_msgs::msg::JointState msg;
    msg.header.frame_id = "";
    msg.header.stamp = this->now();
    
    static const std::vector<std::string> joint_names = {
      "abad_L_Joint", "hip_L_Joint", "knee_L_Joint", "wheel_L_Joint",
      "abad_R_Joint", "hip_R_Joint", "knee_R_Joint", "wheel_R_Joint"
    };
    
    msg.name = joint_names;
    msg.position.resize(8, 0.0);  // All joints at zero position
    msg.velocity.resize(8, 0.0);
    msg.effort.resize(8, 0.0);
    
    joint_state_pub_->publish(msg);
  }

  PointFoot* pf_;
  uint32_t motor_num_;
  std::shared_ptr<RobotCmd> cmd_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
  rclcpp::TimerBase::SharedPtr cmd_timer_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RobotCommandNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
