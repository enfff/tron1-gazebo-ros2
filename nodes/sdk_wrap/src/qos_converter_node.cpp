#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

/**
 * @brief QoS Converter Node for Livox LiDAR PointCloud2 data
 * 
 * Subscribes to /livox/points with BEST_EFFORT QoS and republishes 
 * to /cloud_in with RELIABLE QoS to solve QoS compatibility issues.
 */
class QoSConverterNode : public rclcpp::Node
{
public:
    QoSConverterNode() : Node("qos_converter_node")
    {
        auto best_effort_qos = rclcpp::QoS(10).best_effort();
        auto reliable_qos = rclcpp::QoS(10).reliable();
        
        // Subscriber: /livox/points with BEST_EFFORT QoS
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/livox/points",
            best_effort_qos,
            std::bind(&QoSConverterNode::pointcloud_callback, this, std::placeholders::_1)
        );
        
        // Publisher: /cloud_in with RELIABLE QoS
        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "/cloud_in",
            reliable_qos
        );
    }

private:
    void pointcloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        publisher_->publish(*msg);
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