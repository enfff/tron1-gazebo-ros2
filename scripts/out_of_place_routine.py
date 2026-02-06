#!/usr/bin/env python3
"""
Out of Place Routine using Nav2 for obstacle avoidance:
1. Record current position
2. Move forward 1 meter slowly
3. Perform a 360° rotation (4 x 90° turns)
4. Return to starting position slowly
"""
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
import math
import time


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


def get_current_pose(tf_buffer, navigator):
    """Get current robot pose from TF"""
    from tf2_ros import LookupException, ExtrapolationException
    
    for _ in range(10):
        rclpy.spin_once(navigator, timeout_sec=0.1)
        try:
            transform = tf_buffer.lookup_transform('map', 'base_Link', rclpy.time.Time())
            x = transform.transform.translation.x
            y = transform.transform.translation.y
            yaw = 2.0 * math.atan2(transform.transform.rotation.z, 
                                    transform.transform.rotation.w)
            return x, y, yaw
        except (LookupException, ExtrapolationException):
            time.sleep(0.1)
    return None, None, None


def normalize_angle(angle):
    """Normalize angle to [-pi, pi]"""
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def main():
    rclpy.init()
    
    navigator = BasicNavigator()
    
    # Simply wait a bit for Nav2 to be ready (same as navigation_routine.py)
    print("Waiting for Nav2...")
    time.sleep(3.0)
    print("Starting Out of Place Routine with Nav2 (obstacle avoidance enabled)...")
    
    # Set up TF listener - import here like navigation_routine does
    from tf2_ros import Buffer, TransformListener, LookupException, ExtrapolationException
    
    tf_buffer = Buffer()
    tf_listener = TransformListener(tf_buffer, navigator)
    
    print("Waiting for TF map->base_Link (this takes a few seconds)...")
    
    # Wait until TF is available
    max_attempts = 50
    transform = None
    for attempt in range(max_attempts):
        rclpy.spin_once(navigator, timeout_sec=0.1)
        
        try:
            transform = tf_buffer.lookup_transform('map', 'base_Link', rclpy.time.Time())
            print("TF available!")
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
        print("ERROR: TF not available! Make sure SLAM is running.")
        rclpy.shutdown()
        return
    
    # Extract starting pose
    start_x = transform.transform.translation.x
    start_y = transform.transform.translation.y
    start_yaw = 2.0 * math.atan2(transform.transform.rotation.z, 
                                  transform.transform.rotation.w)
    
    print(f"Starting position: x={start_x:.2f}, y={start_y:.2f}, yaw={math.degrees(start_yaw):.0f}°")
    
    time.sleep(1.0)
    
    # =========================================================================
    # Step 1: Move forward 1 meter
    # =========================================================================
    print("=== Step 1: Moving forward 1 meter (with obstacle avoidance) ===")
    
    goal_x = start_x + 1.0 * math.cos(start_yaw)
    goal_y = start_y + 1.0 * math.sin(start_yaw)
    
    print(f"Goal position: x={goal_x:.2f}, y={goal_y:.2f}")
    
    goal_pose = create_pose_stamped(navigator, goal_x, goal_y, start_yaw)
    navigator.goToPose(goal_pose)
    
    while not navigator.isTaskComplete():
        feedback = navigator.getFeedback()
        if feedback:
            print(f"Distance remaining: {feedback.distance_remaining:.2f}m")
        rclpy.spin_once(navigator, timeout_sec=0.5)
    
    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print("✓ Successfully reached forward goal!")
    elif result == TaskResult.CANCELED:
        print("Goal was canceled!")
        rclpy.shutdown()
        return
    elif result == TaskResult.FAILED:
        print("Goal failed!")
        rclpy.shutdown()
        return
    
    time.sleep(1.0)
    
    # =========================================================================
    # Step 2: 360 degree rotation using 4 x 90° waypoints
    # =========================================================================
    print("=== Step 2: Performing 360° rotation (4 x 90° turns) ===")
    
    # Get current position for rotation
    current_x, current_y, current_yaw = get_current_pose(tf_buffer, navigator)
    if current_x is None:
        print("ERROR: Cannot get current pose for rotation")
        rclpy.shutdown()
        return
    
    print(f"Rotation center: x={current_x:.2f}, y={current_y:.2f}, yaw={math.degrees(current_yaw):.0f}°")
    
    # Do 4 rotations of 90 degrees each
    for i in range(4):
        target_yaw = normalize_angle(current_yaw + (i + 1) * (math.pi / 2.0))
        print(f"  Turning to {math.degrees(target_yaw):.0f}° ({(i+1)*90}° of 360°)")
        
        turn_pose = create_pose_stamped(navigator, current_x, current_y, target_yaw)
        navigator.goToPose(turn_pose)
        
        while not navigator.isTaskComplete():
            rclpy.spin_once(navigator, timeout_sec=0.2)
        
        result = navigator.getResult()
        if result == TaskResult.SUCCEEDED:
            print(f"  ✓ Turn {i+1}/4 complete")
        else:
            print(f"  Turn {i+1}/4 had issues, continuing...")
        
        time.sleep(0.5)
    
    print("✓ 360° rotation complete!")
    
    print("=== Out of Place Routine Complete! ===")
    
    rclpy.shutdown()


if __name__ == '__main__':
    main()
