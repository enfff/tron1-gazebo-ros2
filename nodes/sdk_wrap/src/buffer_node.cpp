#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

// Global variables for easy topic configuration
const std::string INPUT_TOPIC = "input_topic_changeme";   // BEST_EFFORT input topic
const std::string OUTPUT_TOPIC = "output_topic_changeme";  // RELIABLE output topic

/**
 * @brief Buffer Node - Converts BEST_EFFORT QoS to RELIABLE QoS
 * 
 * This node subscribes to a topic with BEST_EFFORT QoS and republishes
 * the data on another topic with RELIABLE QoS.
 */
class BufferNode : public rclcpp::Node
{
public:
    BufferNode() : Node("buffer_node")
    {
        auto best_effort_qos = rclcpp::QoS(rclcpp::KeepLast(10));
        best_effort_qos.reliability(rclcpp::ReliabilityPolicy::BestEffort);
        best_effort_qos.durability(rclcpp::DurabilityPolicy::Volatile);

        auto reliable_qos = rclcpp::QoS(rclcpp::KeepLast(10));
        reliable_qos.reliability(rclcpp::ReliabilityPolicy::Reliable);
        reliable_qos.durability(rclcpp::DurabilityPolicy::Volatile);

        subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            INPUT_TOPIC,
            best_effort_qos,
            std::bind(&BufferNode::topic_callback, this, std::placeholders::_1));

        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            OUTPUT_TOPIC,
            reliable_qos);

        RCLCPP_INFO(this->get_logger(), "Buffer Node started");
        RCLCPP_INFO(this->get_logger(), "Input topic (BEST_EFFORT): %s", INPUT_TOPIC.c_str());
        RCLCPP_INFO(this->get_logger(), "Output topic (RELIABLE): %s", OUTPUT_TOPIC.c_str());
    }

private:
    void topic_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // Simply republish the message with RELIABLE QoS
        publisher_->publish(*msg);
    }

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<BufferNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
