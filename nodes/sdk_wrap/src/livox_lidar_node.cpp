#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>
#include <std_msgs/msg/header.hpp>

/**
 * @brief Livox LiDAR Publisher Node for TRON1 Robot
 * 
 * This node interfaces with the Livox ROS Driver 2 to publish LiDAR point cloud data
 * and IMU data from Livox sensors, integrating them into the TRON1 robot's sensor suite.
 */
class LivoxPublisherNode : public rclcpp::Node
{
public:
    LivoxPublisherNode() : Node("livox_publisher_node")
    {
        // Declare parameters
        this->declare_parameter("frame_id", "livox_frame");
        this->declare_parameter("base_frame_id", "base_link");
        this->declare_parameter("publish_tf", true);
        this->declare_parameter("lidar_x_offset", 0.0);
        this->declare_parameter("lidar_y_offset", 0.0);
        this->declare_parameter("lidar_z_offset", 0.1);
        this->declare_parameter("lidar_roll", 0.0);
        this->declare_parameter("lidar_pitch", 0.0);
        this->declare_parameter("lidar_yaw", 0.0);

        // Get parameters
        frame_id_ = this->get_parameter("frame_id").as_string();
        base_frame_id_ = this->get_parameter("base_frame_id").as_string();
        publish_tf_ = this->get_parameter("publish_tf").as_bool();
        lidar_x_offset_ = this->get_parameter("lidar_x_offset").as_double();
        lidar_y_offset_ = this->get_parameter("lidar_y_offset").as_double();
        lidar_z_offset_ = this->get_parameter("lidar_z_offset").as_double();
        lidar_roll_ = this->get_parameter("lidar_roll").as_double();
        lidar_pitch_ = this->get_parameter("lidar_pitch").as_double();
        lidar_yaw_ = this->get_parameter("lidar_yaw").as_double();

        // Initialize TF broadcaster
        if (publish_tf_)
        {
            tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
        }

        // Subscribe to Livox driver topics
        pointcloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/livox/lidar", 10,
            std::bind(&LivoxPublisherNode::pointcloud_callback, this, std::placeholders::_1));

        imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/livox/imu", 10,
            std::bind(&LivoxPublisherNode::imu_callback, this, std::placeholders::_1));

        // Publishers for processed data
        processed_pointcloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "/tron1/lidar/pointcloud", 10);

        processed_imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>(
            "/tron1/lidar/imu", 10);

        // Timer for publishing TF
        if (publish_tf_)
        {
            tf_timer_ = this->create_wall_timer(
                std::chrono::milliseconds(50),
                std::bind(&LivoxPublisherNode::publish_tf, this));
        }

        RCLCPP_INFO(this->get_logger(), "Livox Publisher Node initialized");
        RCLCPP_INFO(this->get_logger(), "Frame ID: %s", frame_id_.c_str());
        RCLCPP_INFO(this->get_logger(), "Base Frame ID: %s", base_frame_id_.c_str());
        RCLCPP_INFO(this->get_logger(), "Publishing TF: %s", publish_tf_ ? "true" : "false");
    }

private:
    void pointcloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // Process and republish point cloud with TRON1 frame
        auto processed_msg = std::make_shared<sensor_msgs::msg::PointCloud2>(*msg);
        processed_msg->header.frame_id = frame_id_;
        processed_msg->header.stamp = this->get_clock()->now();

        processed_pointcloud_pub_->publish(*processed_msg);

        // Log periodically
        static int count = 0;
        if (++count % 50 == 0)  // Log every 50 messages (~5 seconds at 10Hz)
        {
            RCLCPP_INFO(this->get_logger(), 
                "Published point cloud with %d points", 
                processed_msg->width * processed_msg->height);
        }
    }

    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
    {
        // Process and republish IMU data with TRON1 frame
        auto processed_msg = std::make_shared<sensor_msgs::msg::Imu>(*msg);
        processed_msg->header.frame_id = frame_id_;
        processed_msg->header.stamp = this->get_clock()->now();

        processed_imu_pub_->publish(*processed_msg);

        // Log periodically
        static int count = 0;
        if (++count % 100 == 0)  // Log every 100 messages (~10 seconds at 10Hz)
        {
            RCLCPP_INFO(this->get_logger(), "Published IMU data from Livox sensor");
        }
    }

    void publish_tf()
    {
        if (!tf_broadcaster_)
            return;

        geometry_msgs::msg::TransformStamped t;
        t.header.stamp = this->get_clock()->now();
        t.header.frame_id = base_frame_id_;
        t.child_frame_id = frame_id_;

        // Set translation
        t.transform.translation.x = lidar_x_offset_;
        t.transform.translation.y = lidar_y_offset_;
        t.transform.translation.z = lidar_z_offset_;

        // Set rotation
        tf2::Quaternion q;
        q.setRPY(lidar_roll_, lidar_pitch_, lidar_yaw_);
        t.transform.rotation.x = q.x();
        t.transform.rotation.y = q.y();
        t.transform.rotation.z = q.z();
        t.transform.rotation.w = q.w();

        tf_broadcaster_->sendTransform(t);
    }

    // Parameters
    std::string frame_id_;
    std::string base_frame_id_;
    bool publish_tf_;
    double lidar_x_offset_, lidar_y_offset_, lidar_z_offset_;
    double lidar_roll_, lidar_pitch_, lidar_yaw_;

    // ROS2 components
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr pointcloud_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr processed_pointcloud_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr processed_imu_pub_;
    std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
    rclcpp::TimerBase::SharedPtr tf_timer_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<LivoxPublisherNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}