#!/usr/bin/env python3
"""
ROS2 Navigation Launcher
Sequential launch of sensors, SLAM, and Nav2.
Polls iris-conversation server for navigation requests.
"""

import subprocess
import threading
import time
import signal
import sys
import os
import requests

# ============================================================================
# Configuration
# ============================================================================
CONTAINER = "struzzo-jetson"
ROS_SETUP = "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash"
TIMEOUT = 120

# Iris conversation server URL
# Use localhost since launcher runs on host, not in Docker
IRIS_SERVER_HOST = os.environ.get('IRIS_SERVER_HOST', 'localhost')
IRIS_SERVER_PORT = os.environ.get('IRIS_SERVER_PORT', '2000')
IRIS_SERVER_URL = f'http://{IRIS_SERVER_HOST}:{IRIS_SERVER_PORT}/api/'

SENSOR_TOPICS = [
    "/imu",
    "/odom_raw",
    "/scan",
    "/livox/points",
    "/cloud_in",
    "/tf"
]

NAV2_TOPICS = [
    "/local_costmap/costmap",
    "/global_costmap/costmap"
]

# ============================================================================
# Colors for terminal output
# ============================================================================
class Colors:
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    RED = '\033[0;31m'
    NC = '\033[0m'

def log_info(msg):
    print(f"{Colors.GREEN}[LAUNCHER]{Colors.NC} {msg}")

def log_warn(msg):
    print(f"{Colors.YELLOW}[LAUNCHER]{Colors.NC} {msg}")

def log_error(msg):
    print(f"{Colors.RED}[LAUNCHER]{Colors.NC} {msg}")

# ============================================================================
# Global State
# ============================================================================
class LauncherState:
    def __init__(self):
        self.processes = []
        self.is_running = False
        self.is_ready = False
        self.should_stop = False
        self.lock = threading.Lock()

state = LauncherState()

# ============================================================================
# Iris Server Communication
# ============================================================================
def check_out_of_place_requested():
    """Check if out of place navigation has been requested via iris server"""
    try:
        response = requests.get(f"{IRIS_SERVER_URL}navigation/out_of_place/check", timeout=5)
        if response.status_code == 200:
            data = response.json()
            return data.get('requested', False)
    except requests.exceptions.RequestException:
        pass
    return False

def notify_out_of_place_started():
    """Notify iris server that navigation has started"""
    try:
        response = requests.post(f"{IRIS_SERVER_URL}navigation/out_of_place/start", timeout=5)
        return response.status_code == 200
    except requests.exceptions.RequestException as e:
        log_error(f"Failed to notify start: {e}")
        return False

def notify_out_of_place_completed():
    """Notify iris server that navigation has completed"""
    try:
        response = requests.post(f"{IRIS_SERVER_URL}navigation/out_of_place/complete", timeout=5)
        return response.status_code == 200
    except requests.exceptions.RequestException as e:
        log_error(f"Failed to notify completion: {e}")
        return False

# ============================================================================
# Navigation to Goal Communication
# ============================================================================
def check_nav_goal_requested():
    """Check if navigation to goal has been requested via iris server"""
    try:
        response = requests.get(f"{IRIS_SERVER_URL}navigation/goal/check", timeout=5)
        if response.status_code == 200:
            data = response.json()
            return data.get('requested', False), data.get('coordinates', None)
    except requests.exceptions.RequestException:
        pass
    return False, None

def notify_nav_goal_started():
    """Notify iris server that navigation to goal has started"""
    try:
        response = requests.post(f"{IRIS_SERVER_URL}navigation/goal/start", timeout=5)
        return response.status_code == 200
    except requests.exceptions.RequestException as e:
        log_error(f"Failed to notify nav goal start: {e}")
        return False

def notify_nav_goal_completed():
    """Notify iris server that navigation to goal has completed"""
    try:
        response = requests.post(f"{IRIS_SERVER_URL}navigation/goal/complete", timeout=5)
        return response.status_code == 200
    except requests.exceptions.RequestException as e:
        log_error(f"Failed to notify nav goal completion: {e}")
        return False

# ============================================================================
# Docker Helper Functions
# ============================================================================
def dexec(cmd, background=False):
    """Execute command in container"""
    full_cmd = f'docker exec {CONTAINER} bash -c "{ROS_SETUP} && {cmd}"'
    if background:
        proc = subprocess.Popen(full_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return proc
    else:
        result = subprocess.run(full_cmd, shell=True, capture_output=True, text=True)
        return result

def topic_exists(topic):
    """Check if a ROS2 topic exists"""
    result = dexec("ros2 topic list")
    if result.returncode == 0:
        topics = result.stdout.strip().split('\n')
        return topic in topics
    return False

def wait_for_topic(topic, timeout=TIMEOUT):
    """Wait for a topic to exist"""
    log_info(f"  Waiting for topic: {topic}")
    elapsed = 0
    while elapsed < timeout:
        if topic_exists(topic):
            log_info(f"  ✓ {topic} exists")
            return True
        time.sleep(1)
        elapsed += 1
        if elapsed % 15 == 0:
            log_info(f"  Still waiting for {topic}... ({elapsed}s elapsed)")
    log_error(f"Timeout waiting for topic {topic}")
    return False

def container_running():
    """Check if container is running"""
    result = subprocess.run(
        f"docker ps --format '{{{{.Names}}}}' | grep -q '^{CONTAINER}$'",
        shell=True
    )
    return result.returncode == 0

def cleanup():
    """Kill all ROS2 processes in container"""
    log_warn("Cleaning up processes...")
    
    subprocess.run(f'docker exec {CONTAINER} bash -c "pkill -f \'ros2 launch\' || true"', shell=True, stderr=subprocess.DEVNULL)
    subprocess.run(f'docker exec {CONTAINER} bash -c "pkill -f \'python3.*launch\' || true"', shell=True, stderr=subprocess.DEVNULL)
    subprocess.run(f'docker exec {CONTAINER} bash -c "pkill -f \'_node\' || true"', shell=True, stderr=subprocess.DEVNULL)
    
    time.sleep(2)
    
    subprocess.run(f'docker exec {CONTAINER} bash -c "pkill -9 -f \'ros2\' || true"', shell=True, stderr=subprocess.DEVNULL)
    
    for proc in state.processes:
        try:
            proc.terminate()
        except:
            pass
    
    state.processes.clear()
    state.is_running = False
    state.is_ready = False
    log_info("Cleanup complete")

# ============================================================================
# Launch Functions
# ============================================================================
def launch_system():
    """Launch sensors, SLAM, and Nav2 sequentially"""
    with state.lock:
        if state.is_running:
            return False, "System already running"
        state.is_running = True
        state.is_ready = False
    
    try:
        if not container_running():
            state.is_running = False
            return False, f"Container '{CONTAINER}' is not running!"
        
        log_info(f"Starting ROS2 sequential launch in container '{CONTAINER}'...")
        
        # STAGE 1: Launch sensors
        log_info("Stage 1: Launching sensors...")
        sensors_proc = dexec("ros2 launch sdk_wrap sensors_launch.py", background=True)
        state.processes.append(sensors_proc)
        
        log_info("Waiting for sensor topics...")
        for topic in SENSOR_TOPICS:
            if not wait_for_topic(topic, TIMEOUT):
                cleanup()
                return False, f"Failed to detect topic {topic}"
        
        log_info("✓ All sensor topics are publishing!")
        time.sleep(2)
        
        # STAGE 2: Launch SLAM
        log_info("Stage 2: Launching SLAM toolbox...")
        slam_proc = dexec(
            "ros2 launch slam_toolbox online_async_launch.py "
            "slam_params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/slam_params.yaml",
            background=True
        )
        state.processes.append(slam_proc)
        
        if not wait_for_topic("/map", TIMEOUT):
            cleanup()
            return False, "Failed to detect /map topic"
        
        log_info("✓ SLAM /map topic is publishing!")
        time.sleep(2)
        
        # STAGE 3: Launch Navigation
        log_info("Stage 3: Launching Nav2...")
        nav_proc = dexec(
            "ros2 launch nav2_bringup navigation_launch.py "
            "params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/nav2_params.yaml "
            "map:=/root/limx_ws/map_saves/map_area_2.yaml",
            background=True
        )
        state.processes.append(nav_proc)
        
        log_info("Waiting for Nav2 topics...")
        for topic in NAV2_TOPICS:
            if not wait_for_topic(topic, TIMEOUT):
                cleanup()
                return False, f"Failed to detect topic {topic}"
        
        log_info("✓ Nav2 topics are publishing!")
        
        # ALL DONE
        print(f"\n{Colors.GREEN}============================================{Colors.NC}")
        print(f"{Colors.GREEN}              ALL DONE                      {Colors.NC}")
        print(f"{Colors.GREEN}============================================{Colors.NC}\n")
        log_info("All systems launched and publishing!")
        
        state.is_ready = True
        return True, "System launched successfully"
        
    except Exception as e:
        cleanup()
        return False, f"Launch failed: {str(e)}"

def run_out_of_place_routine():
    """Run the out of place navigation script with live output"""
    log_info("Starting Out of Place Routine...")
    log_info("=" * 50)
    notify_out_of_place_started()
    
    # Run with live output streaming
    full_cmd = f'docker exec {CONTAINER} bash -c "{ROS_SETUP} && python3 /root/limx_ws/src/scripts/out_of_place_routine.py"'
    
    process = subprocess.Popen(
        full_cmd, 
        shell=True, 
        stdout=subprocess.PIPE, 
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )
    
    # Stream output line by line
    for line in process.stdout:
        line = line.rstrip()
        if line:
            print(f"{Colors.GREEN}[ROUTINE]{Colors.NC} {line}")
    
    process.wait()
    
    log_info("=" * 50)
    if process.returncode == 0:
        log_info("Out of Place Routine completed successfully!")
    else:
        log_error(f"Out of Place Routine failed with code: {process.returncode}")
    
    notify_out_of_place_completed()

def run_navigate_to_goal(coordinates):
    """Run the navigate to goal script with coordinates"""
    x = coordinates.get('x', 0.0)
    y = coordinates.get('y', 0.0)
    z = coordinates.get('z', 0.0)
    source_frame = coordinates.get('source_frame', 'zed_frame')
    
    log_info("Starting Navigation to Goal...")
    log_info("=" * 50)
    log_info(f"Target coordinates ({source_frame}): x={x:.3f}, y={y:.3f}, z={z:.3f}")
    notify_nav_goal_started()
    
    # Run with live output streaming
    cmd = f"python3 /root/limx_ws/src/scripts/navigate_to_goal.py --x {x} --y {y} --z {z} --source-frame {source_frame}"
    full_cmd = f'docker exec {CONTAINER} bash -c "{ROS_SETUP} && {cmd}"'
    
    process = subprocess.Popen(
        full_cmd, 
        shell=True, 
        stdout=subprocess.PIPE, 
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )
    
    # Stream output line by line
    for line in process.stdout:
        line = line.rstrip()
        if line:
            print(f"{Colors.GREEN}[NAV_GOAL]{Colors.NC} {line}")
    
    process.wait()
    
    log_info("=" * 50)
    if process.returncode == 0:
        log_info("Navigation to Goal completed successfully!")
    else:
        log_error(f"Navigation to Goal failed with code: {process.returncode}")
    
    notify_nav_goal_completed()

def navigation_request_loop():
    """Main loop that polls for navigation requests"""
    log_info(f"Polling iris server at {IRIS_SERVER_URL} for navigation requests...")
    
    while not state.should_stop:
        if state.is_ready:
            try:
                # Check for out of place requests
                if check_out_of_place_requested():
                    log_info("Out of place request detected!")
                    run_out_of_place_routine()
                
                # Check for navigation to goal requests
                nav_requested, coordinates = check_nav_goal_requested()
                if nav_requested and coordinates:
                    log_info("Navigation to goal request detected!")
                    run_navigate_to_goal(coordinates)
                    
            except Exception as e:
                log_error(f"Error checking navigation requests: {e}")
        
        time.sleep(1)  # Poll every second

# ============================================================================
# Main Entry Point
# ============================================================================
def signal_handler(sig, frame):
    log_warn("Received shutdown signal...")
    state.should_stop = True
    cleanup()
    sys.exit(0)

def main():
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    log_info("=== ROS2 Navigation Launcher ===")
    log_info(f"Iris server: {IRIS_SERVER_URL}")
    
    # Launch the ROS2 system
    success, message = launch_system()
    
    if not success:
        log_error(f"Launch failed: {message}")
        sys.exit(1)
    
    log_info("System ready! Waiting for navigation requests...")
    log_info("Send POST to iris server /api/navigation/out_of_place/trigger to start routine")
    log_info("Press Ctrl+C to stop")
    
    # Start polling for navigation requests
    navigation_request_loop()

if __name__ == '__main__':
    main()
