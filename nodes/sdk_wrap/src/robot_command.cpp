#include <memory>
#include <string>
#include <chrono>
#include <fstream>
#include <atomic>
#include <sstream>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "nlohmann/json.hpp"

// WebSocket client library
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "limxsdk/pointfoot.h"
#include "limxsdk/datatypes.h"

using namespace limxsdk;
using json = nlohmann::json;

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

class RobotCommandNode : public rclcpp::Node {
public:
  RobotCommandNode() : Node("robot_command"), ws_connected_(false) {
    // Initialize last command timestamp
    last_cmd_time_ = this->now();
    last_twist_.linear.x = 0.0;
    last_twist_.angular.z = 0.0;
    
    // Create joint state publisher
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", rclcpp::SensorDataQoS());

    // Use robot IP address
    std::string robot_ip = "10.192.1.2";
    RCLCPP_INFO(this->get_logger(), "Using robot IP: %s", robot_ip.c_str());

    // Connect to robot SDK (for joint states)
    pf_ = PointFoot::getInstance();
    if (!pf_->init(robot_ip.c_str())) {
      RCLCPP_ERROR(this->get_logger(), "Failed to init LimX PointFoot SDK with IP %s", robot_ip.c_str());
      exit(1);
    }
    
    RCLCPP_INFO(this->get_logger(), "SDK initialized successfully for joint state feedback");
    motor_num_ = pf_->getMotorNumber();
    RCLCPP_INFO(this->get_logger(), "Robot has %u motors", motor_num_);

    // Subscribe to robot state and publish as joint states
    RCLCPP_INFO(this->get_logger(), "Subscribing to robot state via SDK...");
    pf_->subscribeRobotState(
      [this](const RobotStateConstPtr &state) {
        RCLCPP_INFO_ONCE(this->get_logger(), "First robot state received! Publishing joint states...");
        publishJointState(*state);
      }
    );
    RCLCPP_INFO(this->get_logger(), "Robot state subscription registered");

    // Initialize WebSocket for sending commands
    initializeWebSocket(robot_ip);

    // Timer to send twist commands via WebSocket at 30Hz
    cmd_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(33),  // ~30Hz
      [this]() {
        sendTwistCommand();
        // Publish fake joint states as fallback
        publishFakeJointState();
      }
    );

    // Subscribe to cmd_vel
    sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", rclcpp::QoS(10),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        auto now = this->now();
        auto time_since_last = (now - last_cmd_time_).seconds();
        
        last_twist_ = *msg;
        last_cmd_time_ = now;
        
        // Warn if commands are arriving too slowly
        if (time_since_last > 0.05 && time_since_last < 10.0) {
          RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                              "cmd_vel arriving at %.1f Hz (should be ~30Hz)",
                              1.0 / time_since_last);
        }
        
        RCLCPP_INFO(this->get_logger(), 
                   "Received cmd_vel: linear.x=%.3f, angular.z=%.3f (dt=%.3fs)",
                   msg->linear.x, msg->angular.z, time_since_last);
      }
    );

    RCLCPP_INFO(this->get_logger(), "robot_command node ready. Sending commands via WebSocket, receiving joint states via SDK");
  }

  ~RobotCommandNode() {
    if (ws_connected_) {
      ws_client_.close(ws_conn_hdl_, websocketpp::close::status::normal, "");
    }
    if (ws_thread_.joinable()) {
      ws_thread_.join();
    }
  }

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

  std::string generate_guid() {
    std::stringstream ss;
    ss << std::hex << std::chrono::system_clock::now().time_since_epoch().count();
    return ss.str();
  }

  void initializeWebSocket(const std::string& robot_ip) {
    // Clear logging to reduce verbosity
    ws_client_.clear_access_channels(websocketpp::log::alevel::all);
    ws_client_.clear_error_channels(websocketpp::log::elevel::all);
    
    ws_client_.init_asio();
    
    ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
      ws_connected_ = true;
      ws_conn_hdl_ = hdl;
      RCLCPP_INFO(this->get_logger(), "✓ WebSocket connected successfully to robot");
      
      // Send walk mode command on connection
      send_websocket_command("request_walk_mode", json::object());
    });
    
    ws_client_.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg) {
      RCLCPP_DEBUG(this->get_logger(), "WS received: %s", msg->get_payload().c_str());
    });
    
    ws_client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
      ws_connected_ = false;
      try {
        auto con = ws_client_.get_con_from_hdl(hdl);
        RCLCPP_ERROR(this->get_logger(), "WebSocket connection FAILED! Error: %s", 
                     con->get_ec().message().c_str());
      } catch (...) {
        RCLCPP_ERROR(this->get_logger(), "WebSocket connection FAILED! (unknown error)");
      }
    });
    
    ws_client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
      ws_connected_ = false;
      try {
        auto con = ws_client_.get_con_from_hdl(hdl);
        RCLCPP_WARN(this->get_logger(), "WebSocket connection closed. Code: %d, Reason: %s",
                    con->get_remote_close_code(), con->get_remote_close_reason().c_str());
      } catch (...) {
        RCLCPP_WARN(this->get_logger(), "WebSocket connection closed");
      }
    });
    
    // Connect to robot WebSocket server
    std::string server_uri = "ws://" + robot_ip + ":5000";
    RCLCPP_INFO(this->get_logger(), "Connecting to WebSocket: %s", server_uri.c_str());
    
    websocketpp::lib::error_code ec;
    auto con = ws_client_.get_connection(server_uri, ec);
    if (ec) {
      RCLCPP_ERROR(this->get_logger(), "Failed to create WebSocket connection: %s", ec.message().c_str());
      return;
    }
    
    ws_client_.connect(con);
    
    // Run WebSocket in separate thread
    ws_thread_ = std::thread([this]() { 
      try {
        ws_client_.run();
        RCLCPP_INFO(this->get_logger(), "WebSocket thread ended");
      } catch (const std::exception& e) {
        RCLCPP_ERROR(this->get_logger(), "WebSocket thread exception: %s", e.what());
      }
    });
    
    RCLCPP_INFO(this->get_logger(), "WebSocket initialization started, waiting for connection...");
  }

  void send_websocket_command(const std::string& title, const json& data) {
    if (!ws_connected_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, 
                          "WebSocket not connected, cannot send command");
      return;
    }
    
    json message = {
      {"accid", "WF_TRON1A_343"},
      {"title", title},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()},
      {"guid", generate_guid()},
      {"data", data}
    };
    
    websocketpp::lib::error_code ec;
    ws_client_.send(ws_conn_hdl_, message.dump(), websocketpp::frame::opcode::text, ec);
    if (ec) {
      RCLCPP_ERROR(this->get_logger(), "WebSocket send error: %s", ec.message().c_str());
    }
  }

  void sendTwistCommand() {
    auto time_since_last = (this->now() - last_cmd_time_).seconds();
    
    geometry_msgs::msg::Twist twist_to_send;
    if (time_since_last > 2.0) {
      // Timeout - send zero velocity for safety
      twist_to_send.linear.x = 0.0;
      twist_to_send.angular.z = 0.0;
    } else {
      twist_to_send = last_twist_;
    }
    
    // Send twist command via WebSocket
    json twist_data = {
      {"x", twist_to_send.linear.x},
      {"y", 0.0},  // lateral velocity (not used for differential drive)
      {"z", twist_to_send.angular.z}
    };
    
    send_websocket_command("request_twist", twist_data);
    
    static int log_counter = 0;
    if (log_counter++ % 300 == 0) {  // Log every 10 seconds at 30Hz
      RCLCPP_INFO(this->get_logger(), 
                 "Sending twist via WebSocket: x=%.3f, z=%.3f",
                 twist_to_send.linear.x, twist_to_send.angular.z);
    }
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
  
  // WebSocket members
  ws_client ws_client_;
  std::thread ws_thread_;
  websocketpp::connection_hdl ws_conn_hdl_;
  bool ws_connected_;
  
  // Store last received command for continuous publishing
  geometry_msgs::msg::Twist last_twist_;
  rclcpp::Time last_cmd_time_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RobotCommandNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
