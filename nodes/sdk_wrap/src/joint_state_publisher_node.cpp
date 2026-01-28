#include <memory>
#include <string>
#include <chrono>
#include <fstream>
#include <atomic>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "nlohmann/json.hpp"

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using std::placeholders::_1;
using namespace std::chrono_literals;
using json = nlohmann::json;

class JointStatePublisherNode : public rclcpp::Node {
public:
  JointStatePublisherNode() : Node("joint_state_publisher") {
    publisher_ = this->create_publisher<sensor_msgs::msg::JointState>("/joint_states", rclcpp::SensorDataQoS());

    // Load robot IP from config file
    std::string robot_ip = loadRobotIpFromConfig();
    if (robot_ip.empty()) {
      RCLCPP_ERROR(this->get_logger(), "Failed to load robot IP from config file");
      return;
    }

    // Initialize LimX SDK and subscribe to robot state using the loaded robot IP
    pf_ = limxsdk::PointFoot::getInstance();
    
    // Note: robot_command node may have already initialized the SDK singleton
    // We still call init() but it should be idempotent
    if (!pf_->init(robot_ip.c_str())) {
      RCLCPP_WARN(this->get_logger(), "SDK init returned false (may already be initialized by another node)");
    } else {
      RCLCPP_INFO(this->get_logger(), "Connected to robot at %s", robot_ip.c_str());
    }

    // Subscribe to robot state updates and publish into ROS 2
    pf_->subscribeRobotState(
      [this](const limxsdk::RobotStateConstPtr &state) {
        RCLCPP_INFO_ONCE(this->get_logger(), "First robot state callback received with %zu joints", state->q.size());
        publishJointState(*state);
        callback_count_++;
      }
    );

    // Create a diagnostic timer to check if we're receiving callbacks
    diagnostic_timer_ = this->create_wall_timer(
      5s,
      [this]() {
        if (callback_count_ > 0) {
          RCLCPP_INFO(this->get_logger(), "Joint states publishing: %zu messages received", callback_count_.load());
        } else {
          RCLCPP_WARN(this->get_logger(), 
            "No robot state callbacks received yet. Robot may not be streaming state data. "
            "Ensure robot is powered on and motors are enabled.");
        }
      }
    );

    RCLCPP_INFO(this->get_logger(), "Joint state publisher ready. Subscribed to robot state. Publishing to /joint_states");
  }

private:
  static uint64_t now_nanoseconds() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
  }

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

  void publishJointState(const limxsdk::RobotState &state) {
    sensor_msgs::msg::JointState msg;

    // Set frame_id (empty string is typical for joint_states)
    msg.header.frame_id = "";
    
    // Timestamp in nanoseconds from SDK
    msg.header.stamp.sec = static_cast<int32_t>(state.stamp / 1000000000ULL);
    msg.header.stamp.nanosec = static_cast<uint32_t>(state.stamp % 1000000000ULL);

    // Joint names for Point-foot robot according to LimX SDK documentation
    // Motor order: 0: abad_L_Joint, 1: hip_L_Joint, 2: knee_L_Joint
    //              3: abad_R_Joint, 4: hip_R_Joint, 5: knee_R_Joint
    static const std::vector<std::string> joint_names = {
      "abad_L_Joint", "hip_L_Joint", "knee_L_Joint",
      "abad_R_Joint", "hip_R_Joint", "knee_R_Joint"
    };

    // Resize vectors to match the number of joints
    size_t num_joints = state.q.size();
    msg.name.resize(num_joints);
    msg.position.resize(num_joints);
    msg.velocity.resize(num_joints);
    msg.effort.resize(num_joints);

    // Fill joint data
    for (size_t i = 0; i < num_joints; ++i) {
      // Use proper joint names if within range, otherwise fallback to generic names
      if (i < joint_names.size()) {
        msg.name[i] = joint_names[i];
      } else {
        msg.name[i] = "joint_" + std::to_string(i);
      }
      msg.position[i] = state.q[i];     // Joint angles in radians
      msg.velocity[i] = state.dq[i];    // Joint velocities in rad/s
      msg.effort[i] = state.tau[i];     // Joint torques in N·m
    }

    publisher_->publish(msg);
    RCLCPP_DEBUG(this->get_logger(), "Published joint states for %zu joints", num_joints);
  }

  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr publisher_;
  limxsdk::PointFoot* pf_;
  rclcpp::TimerBase::SharedPtr diagnostic_timer_;
  std::atomic<size_t> callback_count_{0};
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