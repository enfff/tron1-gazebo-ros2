#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

// WebSocket client library (you'll need to install: apt-get install libwebsocketpp-dev)
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> client;
using json = nlohmann::json;

class RobotCommandWebSocketNode : public rclcpp::Node {
public:
  RobotCommandWebSocketNode() : Node("robot_command_websocket"), connected_(false) {
    // Initialize WebSocket
    ws_client_.init_asio();
    ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
      connected_ = true;
      conn_hdl_ = hdl;
      RCLCPP_INFO(this->get_logger(), "WebSocket connected to robot");
      
      // Send walk mode command on connection
      send_websocket_command("request_walk_mode", {});
    });
    
    ws_client_.set_message_handler([this](websocketpp::connection_hdl, client::message_ptr msg) {
      RCLCPP_DEBUG(this->get_logger(), "WS received: %s", msg->get_payload().c_str());
    });
    
    ws_client_.set_fail_handler([this](websocketpp::connection_hdl) {
      RCLCPP_ERROR(this->get_logger(), "WebSocket connection failed");
      connected_ = false;
    });
    
    ws_client_.set_close_handler([this](websocketpp::connection_hdl) {
      RCLCPP_WARN(this->get_logger(), "WebSocket connection closed");
      connected_ = false;
    });
    
    // Connect to robot WebSocket
    websocketpp::lib::error_code ec;
    auto con = ws_client_.get_connection("ws://10.192.1.2:5000", ec);
    if (ec) {
      RCLCPP_ERROR(this->get_logger(), "WebSocket connection error: %s", ec.message().c_str());
      return;
    }
    ws_client_.connect(con);
    
    // Run WebSocket in separate thread
    ws_thread_ = std::thread([this]() { ws_client_.run(); });
    
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
      }
    );
    
    // Timer to send twist commands at 30Hz
    twist_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(33),  // ~30Hz
      [this]() { send_twist_command(); }
    );
    
    RCLCPP_INFO(this->get_logger(), "robot_command_websocket node ready");
  }
  
  ~RobotCommandWebSocketNode() {
    if (connected_) {
      ws_client_.close(conn_hdl_, websocketpp::close::status::normal, "");
    }
    if (ws_thread_.joinable()) {
      ws_thread_.join();
    }
  }

private:
  void send_websocket_command(const std::string& title, const json& data) {
    if (!connected_) return;
    
    json message = {
      {"accid", "WF_TRON1A_343"},
      {"title", title},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()},
      {"guid", generate_guid()},
      {"data", data}
    };
    
    websocketpp::lib::error_code ec;
    ws_client_.send(conn_hdl_, message.dump(), websocketpp::frame::opcode::text, ec);
    if (ec) {
      RCLCPP_ERROR(this->get_logger(), "WebSocket send error: %s", ec.message().c_str());
    }
  }
  
  void send_twist_command() {
    auto time_since_last = (this->now() - last_cmd_time_).seconds();
    
    geometry_msgs::msg::Twist twist_to_send;
    if (time_since_last > 2.0) {
      // Timeout - send zero velocity
      twist_to_send.linear.x = 0.0;
      twist_to_send.angular.z = 0.0;
    } else {
      twist_to_send = last_twist_;
    }
    
    // Send twist command via WebSocket (matches websocket_client.py format)
    json twist_data = {
      {"x", twist_to_send.linear.x},
      {"y", 0.0},  // lateral velocity (not used for differential drive)
      {"z", twist_to_send.angular.z}
    };
    
    send_websocket_command("request_twist", twist_data);
    
    static int log_counter = 0;
    if (log_counter++ % 300 == 0) {  // Log every 10 seconds
      RCLCPP_INFO(this->get_logger(), 
                 "Sending twist: x=%.3f, z=%.3f",
                 twist_to_send.linear.x, twist_to_send.angular.z);
    }
  }
  
  std::string generate_guid() {
    // Simple GUID generation
    std::stringstream ss;
    ss << std::hex << std::chrono::system_clock::now().time_since_epoch().count();
    return ss.str();
  }

  client ws_client_;
  std::thread ws_thread_;
  websocketpp::connection_hdl conn_hdl_;
  bool connected_;
  
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr twist_timer_;
  
  geometry_msgs::msg::Twist last_twist_;
  rclcpp::Time last_cmd_time_{0, 0, RCL_ROS_TIME};
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RobotCommandWebSocketNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
