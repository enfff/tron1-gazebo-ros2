#!/usr/bin/env python3
"""
Navigate to Goal Script
Takes coordinates in zed_frame, transforms to map frame, and navigates using Nav2.

Usage:
    python3 navigate_to_goal.py --x 1.5 --y 2.0 --z 0.0 [--yaw 0.0]
    
The coordinates are expected in zed_frame and will be transformed to map frame.
"""
import sys
import math
import time
import argparse

# Flush print helper
def log(msg):
    print(msg, flush=True)

log("Starting navigate_to_goal.py...")

try:
    import rclpy
    from geometry_msgs.msg import PoseStamped
    from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
    from tf2_ros import Buffer, TransformListener, LookupException, ExtrapolationException
    log("ROS2 imports successful")
except ImportError as e:
    log(f"ERROR: Failed to import ROS2 modules: {e}")
    sys.exit(1)


def transform_point_to_map(tf_buffer, x, y, z, source_frame='zed_frame'):
    """Transform a point from source_frame to map frame using TF lookup"""
    try:
        # Get the transform from source_frame to map
        transform = tf_buffer.lookup_transform('map', source_frame, rclpy.time.Time(), timeout=rclpy.duration.Duration(seconds=2.0))
        
        # Extract translation
        tx = transform.transform.translation.x
        ty = transform.transform.translation.y
        tz = transform.transform.translation.z
        
        # Extract rotation (quaternion)
        qx = transform.transform.rotation.x
        qy = transform.transform.rotation.y
        qz = transform.transform.rotation.z
        qw = transform.transform.rotation.w
        
        # Apply rotation to point (quaternion rotation)
        # Using simplified rotation for 2D (assuming mostly yaw rotation)
        # Full quaternion rotation: p' = q * p * q^-1
        # For simplicity, we use the rotation matrix approach
        
        # Construct rotation matrix from quaternion
        r00 = 1 - 2*(qy*qy + qz*qz)
        r01 = 2*(qx*qy - qz*qw)
        r02 = 2*(qx*qz + qy*qw)
        r10 = 2*(qx*qy + qz*qw)
        r11 = 1 - 2*(qx*qx + qz*qz)
        r12 = 2*(qy*qz - qx*qw)
        r20 = 2*(qx*qz - qy*qw)
        r21 = 2*(qy*qz + qx*qw)
        r22 = 1 - 2*(qx*qx + qy*qy)
        
        # Apply rotation and translation
        map_x = r00*x + r01*y + r02*z + tx
        map_y = r10*x + r11*y + r12*z + ty
        map_z = r20*x + r21*y + r22*z + tz
        
        return map_x, map_y, map_z
        
    except Exception as e:
        log(f"Transform error: {e}")
        return None, None, None


def create_pose_stamped(navigator, x, y, yaw):
    """Create a PoseStamped message"""
    pose = PoseStamped()
    pose.header.frame_id = 'map'
    pose.header.stamp = navigator.get_clock().now().to_msg()
    pose.pose.position.x = float(x)
    pose.pose.position.y = float(y)
    pose.pose.position.z = 0.0
    
    # Convert yaw to quaternion
    pose.pose.orientation.z = math.sin(yaw / 2.0)
    pose.pose.orientation.w = math.cos(yaw / 2.0)
    
    return pose


def get_current_pose(tf_buffer, navigator):
    """Get current robot pose from TF"""
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


def calculate_yaw_to_goal(current_x, current_y, goal_x, goal_y):
    """Calculate yaw angle to face the goal"""
    return math.atan2(goal_y - current_y, goal_x - current_x)


def wait_for_tf(tf_buffer, navigator, target_frame, source_frame, timeout_sec=30.0):
    """Wait for a TF transform to become available."""
    start_time = time.time()
    while (time.time() - start_time) < timeout_sec:
        rclpy.spin_once(navigator, timeout_sec=0.1)
        try:
            if tf_buffer.can_transform(
                target_frame,
                source_frame,
                rclpy.time.Time(),
                timeout=rclpy.duration.Duration(seconds=1.0),
            ):
                return True
        except Exception:
            pass
        time.sleep(0.1)
    return False



def main():
    parser = argparse.ArgumentParser(description='Navigate to goal coordinates')
    parser.add_argument('--x', type=float, required=True, help='X coordinate in zed_frame')
    parser.add_argument('--y', type=float, required=True, help='Y coordinate in zed_frame')
    parser.add_argument('--z', type=float, default=0.0, help='Z coordinate in zed_frame')
    parser.add_argument('--yaw', type=float, default=None, help='Target yaw in radians (optional, auto-calculated if not provided)')
    parser.add_argument('--source-frame', type=str, default='zed_frame', help='Source frame for coordinates')
    args = parser.parse_args()
    
    log("=" * 60)
    log("NAVIGATE TO GOAL")
    log("=" * 60)
    log(f"Input coordinates ({args.source_frame}):")
    log(f"  x={args.x:.3f}, y={args.y:.3f}, z={args.z:.3f}")
    rclpy.init()
    
    navigator = BasicNavigator()
    
    # Wait for Nav2
    log("\nWaiting for Nav2...")
    time.sleep(3.0)
    
    # Set up TF listener
    tf_buffer = Buffer()
    tf_listener = TransformListener(tf_buffer, navigator)
    
    log("Waiting for TF transforms...")
    
    tf_ready = True
    if args.source_frame != 'map':
        tf_ready = wait_for_tf(tf_buffer, navigator, 'map', args.source_frame, timeout_sec=40.0)
        if not tf_ready:
            log("ERROR: TF map -> source_frame not available!")
            log(f"Missing transform: map -> {args.source_frame}")
    
    if tf_ready:
        tf_ready = wait_for_tf(tf_buffer, navigator, 'map', 'base_Link', timeout_sec=40.0)
        if not tf_ready:
            log("ERROR: TF map -> base_Link not available!")
    
    if not tf_ready:
        log("Available TF frames:")
        try:
            log(tf_buffer.all_frames_as_string())
        except Exception as e:
            log(f"Could not read TF frames: {e}")
        rclpy.shutdown()
        return 1
    
    log("TF transforms available!")
    
    # Transform coordinates from zed_frame to map frame
    log(f"\nTransforming coordinates from {args.source_frame} to map...")
    map_x, map_y, map_z = transform_point_to_map(
        tf_buffer, args.x, args.y, args.z, args.source_frame
    )
    
    if map_x is None:
        log("ERROR: Failed to transform coordinates!")
        rclpy.shutdown()
        return 1
    
    log(f"Transformed coordinates (map frame):")
    log(f"  x={map_x:.3f}, y={map_y:.3f}, z={map_z:.3f}")
    
    # Get current robot pose
    current_x, current_y, current_yaw = get_current_pose(tf_buffer, navigator)
    if current_x is None:
        log("ERROR: Cannot get current robot pose!")
        rclpy.shutdown()
        return 1
    
    log(f"\nCurrent robot position:")
    log(f"  x={current_x:.3f}, y={current_y:.3f}, yaw={math.degrees(current_yaw):.1f}°")
    
    # Calculate distance to target
    distance_to_target = math.sqrt((map_x - current_x)**2 + (map_y - current_y)**2)
    log(f"\nDistance to target: {distance_to_target:.2f}m")
    
    # Determine yaw - face the target
    if args.yaw is not None:
        target_yaw = args.yaw
        log(f"Using specified yaw: {math.degrees(target_yaw):.1f}°")
    else:
        # Face towards the actual target
        target_yaw = calculate_yaw_to_goal(current_x, current_y, map_x, map_y)
        log(f"Auto-calculated yaw to face target: {math.degrees(target_yaw):.1f}°")
    
    # Navigate to target (Nav2 will stop at safe distance based on costmaps)
    log("\n" + "=" * 60)
    log("STARTING NAVIGATION")
    log("=" * 60)
    
    goal_pose = create_pose_stamped(navigator, map_x, map_y, target_yaw)
    navigator.goToPose(goal_pose)
    
    # Monitor progress
    while not navigator.isTaskComplete():
        feedback = navigator.getFeedback()
        if feedback:
            remaining = feedback.distance_remaining
            log(f"  Distance remaining: {remaining:.2f}m")
        rclpy.spin_once(navigator, timeout_sec=0.5)
    
    # Check result
    result = navigator.getResult()
    log("\n" + "=" * 60)
    
    if result == TaskResult.SUCCEEDED:
        log("✓ NAVIGATION SUCCEEDED!")
        log(f"  Reached goal at x={map_x:.2f}, y={map_y:.2f}")
        rclpy.shutdown()
        return 0
    elif result == TaskResult.CANCELED:
        log("✗ NAVIGATION CANCELED")
        rclpy.shutdown()
        return 1
    elif result == TaskResult.FAILED:
        log("✗ NAVIGATION FAILED")
        rclpy.shutdown()
        return 1
    else:
        log(f"✗ NAVIGATION UNKNOWN RESULT: {result}")
        rclpy.shutdown()
        return 1


if __name__ == '__main__':
    sys.exit(main())
