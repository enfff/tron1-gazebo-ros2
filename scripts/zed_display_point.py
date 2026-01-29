#!/usr/bin/env python3
"""
Transform a point from zed_frame to map_frame and publish as clicked point
"""
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PointStamped
from tf2_ros import Buffer, TransformListener
import tf2_geometry_msgs  # Required for transforming geometry_msgs
from rclpy.duration import Duration
import sys

class ZedPointVisualizer(Node):
    def __init__(self, x, y, z):
        super().__init__('zed_point_visualizer')
        
        # TF setup
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        
        # Publisher for clicked point
        self.point_pub = self.create_publisher(PointStamped, '/clicked_point', 10)
        
        # Store point
        self.zed_point = (x, y, z)
        
        # Timer to keep trying transformation
        self.timer = self.create_timer(1.0, self.transform_and_publish)
        
        self.get_logger().info(f'Transforming point {self.zed_point} from zed_frame to map...')
    
    def transform_and_publish(self):
        try:
            # Check if transform is available
            if not self.tf_buffer.can_transform('map', 'zed_frame', rclpy.time.Time()):
                self.get_logger().warn('Waiting for transform map -> zed_frame...')
                return
            
            # Create point in zed_frame
            point_zed = PointStamped()
            point_zed.header.frame_id = 'zed_frame'
            point_zed.header.stamp = self.get_clock().now().to_msg()
            point_zed.point.x = self.zed_point[0]
            point_zed.point.y = self.zed_point[1]
            point_zed.point.z = self.zed_point[2]
            
            # Transform to map frame
            point_map = self.tf_buffer.transform(point_zed, 'map', Duration(seconds=1.0))
            
            # Publish to clicked_point
            self.point_pub.publish(point_map)
            
            self.get_logger().info(
                f'Point in zed_frame: ({self.zed_point[0]:.2f}, {self.zed_point[1]:.2f}, {self.zed_point[2]:.2f})'
            )
            self.get_logger().info(
                f'Transformed to map: ({point_map.point.x:.2f}, {point_map.point.y:.2f}, {point_map.point.z:.2f})'
            )
            self.get_logger().info('Point published to /clicked_point - visualize in RViz!')
            
            # Stop timer after successful publish
            self.timer.cancel()
            
        except Exception as e:
            self.get_logger().error(f'Transform failed: {e}')

def main(args=None):
    # Scene graph object: whiteboard center
    x, y, z = 1.7626181289335494, -0.491623785562349, 0.7777482904372297
    # Allow command line override
    if args and len(args) >= 4:
        try:
            x = float(args[1])
            y = float(args[2])
            z = float(args[3])
        except ValueError:
            print("Usage: python3 zed_display_point.py [x] [y] [z]")
            return
    
    rclpy.init()
    
    node = ZedPointVisualizer(x, y, z)
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main(sys.argv)
