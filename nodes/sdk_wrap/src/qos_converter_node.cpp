#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

/**
 * @brief QoS Converter Node for Livox LiDAR PointCloud2 data
 * 
 * Subscribes to /livox/points with RELIABLE QoS (matching Livox publisher)
 * and republishes to /cloud_in with RELIABLE QoS for compatibility.
 * RE-STAMPS messages with current time to fix TF timing issues.
 */
class QoSConverterNode : public rclcpp::Node
{
public:
    QoSConverterNode() : Node("qos_converter_node")
    {
        // Match Livox publisher QoS: RELIABLE with depth 256
        auto reliable_qos_sub = rclcpp::QoS(256).reliable();
        auto reliable_qos_pub = rclcpp::QoS(10).reliable();
        
        // Subscriber: /livox/points with RELIABLE QoS (matching publisher)
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/livox/points",
            reliable_qos_sub,
            std::bind(&QoSConverterNode::pointcloud_callback, this, std::placeholders::_1)
        );
        
        // Publisher: /cloud_in with RELIABLE QoS
        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "/cloud_in",
            reliable_qos_pub
        );
        
        RCLCPP_INFO(this->get_logger(), "QoS Converter initialized: /livox/points (RELIABLE) -> /cloud_in (RELIABLE) with RE-STAMPING");
    }

private:
    void pointcloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // Create a copy and re-stamp with current time to fix TF timing
        auto output = *msg;
        output.header.stamp = this->now();
        publisher_->publish(output);
    }
    
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<QoSConverterNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}