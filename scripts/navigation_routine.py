#!/usr/bin/env python3
"""
Navigation routine: Go forward 3 meters and return to start
"""
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
import math

def create_pose_stamped(navigator, x, y, yaw):
    """Create a PoseStamped message"""
    pose = PoseStamped()
    pose.header.frame_id = 'map'
    pose.header.stamp = navigator.get_clock().now().to_msg()
    pose.pose.position.x = x
    pose.pose.position.y = y
    pose.pose.position.z = 0.0
    
    # Convert yaw to quaternion
    pose.pose.orientation.z = math.sin(yaw / 2.0)
    pose.pose.orientation.w = math.cos(yaw / 2.0)
    
    return pose

def main():
    rclpy.init()
    
    navigator = BasicNavigator()
    
    # Simply wait a bit for Nav2 to be ready
    print("Waiting for Nav2...")
    import time
    time.sleep(3.0)
    print("Starting navigation routine...")
    
    # Get current pose from TF (slam_toolbox provides map->odom->base_link)
    from rclpy.duration import Duration
    from tf2_ros import Buffer, TransformListener, LookupException, ExtrapolationException
    
    tf_buffer = Buffer()
    tf_listener = TransformListener(tf_buffer, navigator)
    
    print("Waiting for TF map->base_Link (this takes a few seconds)...")
    
    # Wait until TF is available (spin to let listener receive messages)
    max_attempts = 50
    transform = None
    for attempt in range(max_attempts):
        rclpy.spin_once(navigator, timeout_sec=0.1)  # Let TF listener receive data
        
        try:
            # Try to get the transform
            transform = tf_buffer.lookup_transform('map', 'base_Link', rclpy.time.Time())
            print(f"TF available!")
            break
        except (LookupException, ExtrapolationException) as e:
            if attempt % 10 == 0:
                print(f"Still waiting for TF... {attempt+1}/{max_attempts}")
            time.sleep(0.2)
        except Exception as e:
            if attempt % 10 == 0:
                print(f"Waiting... ({e})")
            time.sleep(0.2)
    
    if transform is None:
        print("ERROR: TF not available after maximum attempts!")
        print("Make sure slam_toolbox is running and publishing the map")
        return
    
    # Extract pose from transform
    try:
        start_x = transform.transform.translation.x
        start_y = transform.transform.translation.y
        start_yaw = 2.0 * math.atan2(transform.transform.rotation.z, 
                                      transform.transform.rotation.w)
    except Exception as e:
        print(f"ERROR: Could not get current pose from TF: {e}")
        print("Make sure slam_toolbox is running and publishing map->base_Link transform!")
        return
    
    print(f"Starting position: x={start_x:.2f}, y={start_y:.2f}, yaw={start_yaw:.2f}")
    
    # Calculate goal 3 meters forward in robot's current direction
    goal_x = start_x + 3.0 * math.cos(start_yaw)
    goal_y = start_y + 3.0 * math.sin(start_yaw)
    
    print(f"Goal position: x={goal_x:.2f}, y={goal_y:.2f}")
    
    # Create goal pose (3m forward)
    goal_pose = create_pose_stamped(navigator, goal_x, goal_y, start_yaw)
    
    # Send navigation goal
    print("Sending navigation goal: 3 meters forward...")
    navigator.goToPose(goal_pose)
    
    # Wait for navigation to complete
    while not navigator.isTaskComplete():
        feedback = navigator.getFeedback()
        if feedback:
            print(f"Distance remaining: {feedback.distance_remaining:.2f}m")
        rclpy.spin_once(navigator, timeout_sec=0.1)
    
    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print("Successfully reached forward goal!")
    elif result == TaskResult.CANCELED:
        print("Goal was canceled!")
        return
    elif result == TaskResult.FAILED:
        print("Goal failed!")
        return
    
    # Return to starting point
    print(f"Returning to start: x={start_x:.2f}, y={start_y:.2f}")
    return_pose = create_pose_stamped(navigator, start_x, start_y, start_yaw)
    
    navigator.goToPose(return_pose)
    
    while not navigator.isTaskComplete():
        feedback = navigator.getFeedback()
        if feedback:
            print(f"Distance remaining: {feedback.distance_remaining:.2f}m")
        rclpy.spin_once(navigator, timeout_sec=0.1)
    
    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print("Successfully returned to start!")
    elif result == TaskResult.CANCELED:
        print("Return was canceled!")
    elif result == TaskResult.FAILED:
        print("Return failed!")
    
    print("Navigation routine complete!")
    
    rclpy.shutdown()

if __name__ == '__main__':
    main()
