#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <fstream>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using json = nlohmann::json;
using websocketpp::client;
using websocketpp::connection_hdl;

class ImuPublisherNode : public rclcpp::Node {
public:
  ImuPublisherNode() : Node("imu_publisher"), accid_(""), connected_(false) {
    publisher_ = this->create_publisher<sensor_msgs::msg::Imu>("/imu", rclcpp::SensorDataQoS());

    // Initialize WebSocket client
    ws_client_.init_asio();
    ws_client_.set_open_handler([this](connection_hdl hdl) { on_open(hdl); });
    ws_client_.set_message_handler([this](connection_hdl hdl, client<websocketpp::config::asio>::message_ptr msg) {
      on_message(hdl, msg);
    });
    ws_client_.set_close_handler([this](connection_hdl hdl) { on_close(hdl); });

    // Connect to robot WebSocket server
    std::string robot_ip = "10.192.1.2";
    std::string server_uri = std::string("ws://") + robot_ip + ":5000";
    websocketpp::lib::error_code ec;
    auto con = ws_client_.get_connection(server_uri, ec);

    if (ec) {
      RCLCPP_ERROR(this->get_logger(), "Connection error: %s", ec.message().c_str());
      return;
    }

    current_hdl_ = con->get_handle();
    ws_client_.connect(con);

    // Run WebSocket client in separate thread
    ws_thread_ = std::thread([this]() {
      try {
        ws_client_.run();
      } catch (const std::exception& e) {
        RCLCPP_ERROR(this->get_logger(), "WebSocket error: %s", e.what());
      }
    });

    RCLCPP_INFO(this->get_logger(), "IMU publisher connecting to %s", server_uri.c_str());
  }

  ~ImuPublisherNode() {
    if (connected_) {
      ws_client_.close(current_hdl_, websocketpp::close::status::normal, "Node shutdown");
    }
    ws_client_.stop();
    if (ws_thread_.joinable()) {
      ws_thread_.join();
    }
  }

private:
  void on_open(connection_hdl hdl) {
    connected_ = true;
    current_hdl_ = hdl;
    RCLCPP_INFO(this->get_logger(), "Connected to IMU WebSocket");
  }

  void on_message(connection_hdl hdl, client<websocketpp::config::asio>::message_ptr msg) {
    try {
      json data = json::parse(msg->get_payload());

      // Extract accid if present and not yet set
      if (data.contains("accid") && data["accid"].is_string() && accid_.empty()) {
        accid_ = data["accid"].get<std::string>();
        // Send enable request for IMU data
        send_request("request_enable_imu", {{"enable", true}});
      }

      // Check if this is IMU data
      if (data.contains("title") && data["title"] == "notify_imu") {
        if (data.contains("data")) {
          publish_imu(data["data"]);
        }
      }
    } catch (const std::exception& e) {
      // Silently ignore parse errors
    }
  }

  void on_close(connection_hdl hdl) {
    connected_ = false;
    RCLCPP_WARN(this->get_logger(), "WebSocket connection closed");
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

  void publish_imu(const json& imu_data) {
    sensor_msgs::msg::Imu msg;

    // Header
    msg.header.stamp = this->now();
    msg.header.frame_id = "imu";

    // Parse orientation [x, y, z, w]
    if (imu_data.contains("orientation") && imu_data["orientation"].is_array()) {
      auto quat = imu_data["orientation"];
      msg.orientation.x = quat[0].get<double>();
      msg.orientation.y = quat[1].get<double>();
      msg.orientation.z = quat[2].get<double>();
      msg.orientation.w = quat[3].get<double>();
    }

    // Parse angular velocity [x, y, z] in rad/s
    if (imu_data.contains("gyro") && imu_data["gyro"].is_array()) {
      auto gyro = imu_data["gyro"];
      msg.angular_velocity.x = gyro[0].get<double>();
      msg.angular_velocity.y = gyro[1].get<double>();
      msg.angular_velocity.z = gyro[2].get<double>();
    }

    // Parse linear acceleration [x, y, z] in m/s^2
    if (imu_data.contains("acc") && imu_data["acc"].is_array()) {
      auto acc = imu_data["acc"];
      msg.linear_acceleration.x = acc[0].get<double>();
      msg.linear_acceleration.y = acc[1].get<double>();
      msg.linear_acceleration.z = acc[2].get<double>();
    }

    // Set proper covariances for robot_localization EKF
    // Orientation covariance (roll, pitch, yaw in rad^2)
    msg.orientation_covariance[0] = 0.1;   // roll variance
    msg.orientation_covariance[4] = 0.1;   // pitch variance
    msg.orientation_covariance[8] = 0.05;  // yaw variance (IMU is good at yaw)
    
    // Angular velocity covariance (rad/s)^2
    msg.angular_velocity_covariance[0] = 0.02;  // x
    msg.angular_velocity_covariance[4] = 0.02;  // y
    msg.angular_velocity_covariance[8] = 0.02;  // z
    
    // Linear acceleration covariance (m/s^2)^2
    msg.linear_acceleration_covariance[0] = 0.1;  // x
    msg.linear_acceleration_covariance[4] = 0.1;  // y
    msg.linear_acceleration_covariance[8] = 0.1;  // z

    publisher_->publish(msg);
  }

  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr publisher_;
  client<websocketpp::config::asio> ws_client_;
  connection_hdl current_hdl_;
  std::thread ws_thread_;
  std::string accid_;
  std::atomic<bool> connected_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ImuPublisherNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}