#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <fstream>

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using json = nlohmann::json;
using websocketpp::client;
using websocketpp::connection_hdl;

class OdomPublisherNode : public rclcpp::Node {
public:
  OdomPublisherNode() : Node("odom_publisher"), accid_(""), connected_(false) {
    publisher_ = this->create_publisher<nav_msgs::msg::Odometry>("/odom", rclcpp::SensorDataQoS());

    // Load robot IP from config file
    std::string robot_ip = loadRobotIpFromConfig();
    if (robot_ip.empty()) {
      RCLCPP_ERROR(this->get_logger(), "Failed to load robot IP from config file");
      return;
    }

    // Initialize WebSocket client
    ws_client_.init_asio();
    ws_client_.set_open_handler([this](connection_hdl hdl) { on_open(hdl); });
    ws_client_.set_message_handler([this](connection_hdl hdl, client<websocketpp::config::asio>::message_ptr msg) {
      on_message(hdl, msg);
    });
    ws_client_.set_close_handler([this](connection_hdl hdl) { on_close(hdl); });

    // Connect to robot WebSocket server
    std::string server_uri = "ws://" + robot_ip + ":5000";
    websocketpp::lib::error_code ec;
    auto con = ws_client_.get_connection(server_uri, ec);

    if (ec) {
      return;
    }

    current_hdl_ = con->get_handle();
    ws_client_.connect(con);

    // Run WebSocket client in separate thread
    ws_thread_ = std::thread([this]() {
      try {
        ws_client_.run();
      } catch (const std::exception& e) {
        // Silently handle errors
      }
    });
  }

  ~OdomPublisherNode() {
    if (connected_) {
      ws_client_.close(current_hdl_, websocketpp::close::status::normal, "Node shutdown");
    }
    ws_client_.stop();
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

  void on_open(connection_hdl hdl) {
    connected_ = true;
    current_hdl_ = hdl;
    RCLCPP_INFO(this->get_logger(), "Connected");
  }

  void on_message(connection_hdl hdl, client<websocketpp::config::asio>::message_ptr msg) {
    try {
      json data = json::parse(msg->get_payload());

      // Extract accid if present and not yet set
      if (data.contains("accid") && data["accid"].is_string() && accid_.empty()) {
        accid_ = data["accid"].get<std::string>();
        // Now that we have accid, send enable request
        send_request("request_enable_odom", {{"enable", true}});
      }

      // Check if this is odometry data
      if (data.contains("title") && data["title"] == "notify_odom") {
        if (data.contains("data")) {
          publish_odom(data["data"]);
        }
      }
    } catch (const std::exception& e) {
      // Silently ignore parse errors
    }
  }

  void on_close(connection_hdl hdl) {
    connected_ = false;
  }

  void send_request(const std::string& title, const json& data = json::object()) {
    if (!connected_) {
      return;
    }

    json message;
    message["accid"] = accid_;
    message["title"] = title;
    message["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch()).count();
    message["guid"] = generate_guid();
    message["data"] = data;

    std::string message_str = message.dump();
    ws_client_.send(current_hdl_, message_str, websocketpp::frame::opcode::text);
  }

  std::string generate_guid() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid u = gen();
    return boost::uuids::to_string(u);
  }

  void publish_odom(const json& odom_data) {
    nav_msgs::msg::Odometry msg;

    // Header
    msg.header.stamp = this->now();
    msg.header.frame_id = "odom";
    msg.child_frame_id = "base_link";

    // Parse pose orientation [x, y, z, w]
    if (odom_data.contains("pose_orientation") && odom_data["pose_orientation"].is_array()) {
      auto quat = odom_data["pose_orientation"];
      msg.pose.pose.orientation.x = quat[0].get<double>();
      msg.pose.pose.orientation.y = quat[1].get<double>();
      msg.pose.pose.orientation.z = quat[2].get<double>();
      msg.pose.pose.orientation.w = quat[3].get<double>();
    }

    // Parse pose position [x, y, z] in meters
    if (odom_data.contains("pose_position") && odom_data["pose_position"].is_array()) {
      auto pos = odom_data["pose_position"];
      msg.pose.pose.position.x = pos[0].get<double>();
      msg.pose.pose.position.y = pos[1].get<double>();
      msg.pose.pose.position.z = pos[2].get<double>();
    }

    // Parse twist linear [x, y, z] in m/s
    if (odom_data.contains("twist_linear") && odom_data["twist_linear"].is_array()) {
      auto lin = odom_data["twist_linear"];
      msg.twist.twist.linear.x = lin[0].get<double>();
      msg.twist.twist.linear.y = lin[1].get<double>();
      msg.twist.twist.linear.z = lin[2].get<double>();
    }

    // Parse twist angular [x, y, z] in rad/s
    if (odom_data.contains("twist_angular") && odom_data["twist_angular"].is_array()) {
      auto ang = odom_data["twist_angular"];
      msg.twist.twist.angular.x = ang[0].get<double>();
      msg.twist.twist.angular.y = ang[1].get<double>();
      msg.twist.twist.angular.z = ang[2].get<double>();
    }

    // Set covariances to unknown
    msg.pose.covariance[0] = -1.0;
    msg.twist.covariance[0] = -1.0;

    publisher_->publish(msg);
  }

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr publisher_;
  client<websocketpp::config::asio> ws_client_;
  connection_hdl current_hdl_;
  std::thread ws_thread_;
  std::string accid_;
  std::atomic<bool> connected_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<OdomPublisherNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
