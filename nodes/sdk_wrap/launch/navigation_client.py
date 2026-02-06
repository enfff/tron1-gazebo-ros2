#!/usr/bin/env python3
"""
Navigation Client API
Functions to communicate with the ROS2 Navigation Launcher Server.
"""

import requests
import os
from urllib.parse import urljoin

def detect_docker_environment():
    """Detect if we're running inside a Docker container"""
    try:
        if os.path.exists('/.dockerenv'):
            return True
        if os.path.exists('/proc/1/cgroup'):
            with open('/proc/1/cgroup', 'r') as f:
                content = f.read()
                if 'docker' in content or 'containerd' in content:
                    return True
        current_path = os.getcwd()
        if current_path.startswith('/app') or '/app/' in current_path:
            return True
    except Exception:
        pass
    return False

# Determine host based on environment
if detect_docker_environment():
    HOST = 'nav-server'  # Use container name when inside Docker
    print("Detected Docker environment - using container name 'nav-server'")
else:
    HOST = 'localhost'   # Use localhost when on host system
    print("Detected host environment - using 'localhost'")

PORT = '3000'
URL_SERVER = f'http://{HOST}:{PORT}/api/'

# API Endpoints
API_STATUS = urljoin(URL_SERVER, 'status')
API_LAUNCH = urljoin(URL_SERVER, 'launch')
API_SHUTDOWN = urljoin(URL_SERVER, 'shutdown')
API_NAV_OUT_OF_PLACE = urljoin(URL_SERVER, 'navigate/out_of_place')
API_NAV_FORWARD = urljoin(URL_SERVER, 'navigate/forward')
API_NAV_GOAL = urljoin(URL_SERVER, 'navigate/goal')


def get_status():
    """Get current system status"""
    try:
        response = requests.get(API_STATUS, timeout=5)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"Error: {response.status_code}")
            return None
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return None


def is_ready():
    """Check if the navigation system is ready"""
    status = get_status()
    if status:
        return status.get('is_ready', False)
    return False


def is_navigation_in_progress():
    """Check if navigation is currently in progress"""
    status = get_status()
    if status:
        return status.get('navigation_in_progress', False)
    return False


def launch_system():
    """Launch the ROS2 navigation system"""
    try:
        response = requests.post(API_LAUNCH, timeout=5)
        if response.status_code == 200:
            result = response.json()
            print(f"Launch: {result.get('message', 'OK')}")
            return True
        else:
            print(f"Launch failed: {response.status_code} - {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return False


def shutdown_system():
    """Shutdown the ROS2 navigation system"""
    try:
        response = requests.post(API_SHUTDOWN, timeout=10)
        if response.status_code == 200:
            result = response.json()
            print(f"Shutdown: {result.get('message', 'OK')}")
            return True
        else:
            print(f"Shutdown failed: {response.status_code} - {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return False


def navigate_out_of_place():
    """Run the out of place routine"""
    try:
        response = requests.post(API_NAV_OUT_OF_PLACE, timeout=5)
        if response.status_code == 200:
            result = response.json()
            print(f"Out of place: {result.get('message', 'OK')}")
            return True
        else:
            result = response.json()
            print(f"Out of place failed: {result.get('message', response.text)}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return False


def navigate_forward(distance=3.0):
    """Navigate forward by specified distance"""
    try:
        payload = {'distance': distance}
        response = requests.post(API_NAV_FORWARD, json=payload, timeout=5)
        if response.status_code == 200:
            result = response.json()
            print(f"Navigate forward: {result.get('message', 'OK')}")
            return True
        else:
            result = response.json()
            print(f"Navigate forward failed: {result.get('message', response.text)}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return False


def navigate_to_goal(x, y, yaw=0.0):
    """Navigate to a specific goal position"""
    try:
        payload = {'x': x, 'y': y, 'yaw': yaw}
        response = requests.post(API_NAV_GOAL, json=payload, timeout=5)
        if response.status_code == 200:
            result = response.json()
            print(f"Navigate to goal: {result.get('message', 'OK')}")
            return True
        else:
            result = response.json()
            print(f"Navigate to goal failed: {result.get('message', response.text)}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        return False


def wait_for_ready(timeout=180):
    """Wait until the system is ready"""
    import time
    elapsed = 0
    while elapsed < timeout:
        if is_ready():
            print("System is ready!")
            return True
        time.sleep(2)
        elapsed += 2
        if elapsed % 10 == 0:
            print(f"Waiting for system to be ready... ({elapsed}s)")
    print("Timeout waiting for system to be ready")
    return False


def wait_for_navigation_complete(timeout=300):
    """Wait until navigation is complete"""
    import time
    elapsed = 0
    while elapsed < timeout:
        if not is_navigation_in_progress():
            print("Navigation complete!")
            return True
        time.sleep(2)
        elapsed += 2
        if elapsed % 10 == 0:
            print(f"Waiting for navigation to complete... ({elapsed}s)")
    print("Timeout waiting for navigation to complete")
    return False


# ============================================================================
# Command line interface
# ============================================================================
if __name__ == '__main__':
    import sys
    import argparse
    
    parser = argparse.ArgumentParser(description='Navigation Client CLI')
    parser.add_argument('command', choices=['status', 'launch', 'shutdown', 'out_of_place', 'forward', 'goal'],
                        help='Command to execute')
    parser.add_argument('--x', type=float, help='X coordinate for goal')
    parser.add_argument('--y', type=float, help='Y coordinate for goal')
    parser.add_argument('--yaw', type=float, default=0.0, help='Yaw angle for goal')
    parser.add_argument('--distance', type=float, default=3.0, help='Distance for forward navigation')
    parser.add_argument('--wait', action='store_true', help='Wait for operation to complete')
    
    args = parser.parse_args()
    
    if args.command == 'status':
        status = get_status()
        if status:
            print(f"Running: {status['is_running']}")
            print(f"Ready: {status['is_ready']}")
            print(f"Navigation in progress: {status['navigation_in_progress']}")
            if status['current_navigation']:
                print(f"Current navigation: {status['current_navigation']}")
    
    elif args.command == 'launch':
        launch_system()
        if args.wait:
            wait_for_ready()
    
    elif args.command == 'shutdown':
        shutdown_system()
    
    elif args.command == 'out_of_place':
        navigate_out_of_place()
        if args.wait:
            wait_for_navigation_complete()
    
    elif args.command == 'forward':
        navigate_forward(args.distance)
        if args.wait:
            wait_for_navigation_complete()
    
    elif args.command == 'goal':
        if args.x is None or args.y is None:
            print("Error: --x and --y are required for goal navigation")
            sys.exit(1)
        navigate_to_goal(args.x, args.y, args.yaw)
        if args.wait:
            wait_for_navigation_complete()
