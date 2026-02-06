#!/bin/bash

# Sequential ROS2 Launch Script with Topic Monitoring
# This script launches three ROS2 processes sequentially, waiting for specific
# topics to be publishing before proceeding to the next launch.
# All commands run inside the Docker container.

set -e

# Docker container name
CONTAINER="struzzo-jetson"

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[LAUNCHER]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[LAUNCHER]${NC} $1"
}

log_error() {
    echo -e "${RED}[LAUNCHER]${NC} $1"
}

# Helper to run commands in container
dexec() {
    docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash && $*"
}

# Function to wait for a topic to be publishing
# Usage: wait_for_topic <topic_name> <timeout_seconds>
wait_for_topic() {
    local topic=$1
    local timeout=$2
    local elapsed=0
    
    log_info "  Waiting for topic: $topic"
    
    while [ $elapsed -lt $timeout ]; do
        # Check if topic exists in the topic list
        if docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && ros2 topic list" 2>/dev/null | grep -q "^${topic}$"; then
            log_info "  ✓ $topic exists"
            return 0
        fi
        
        sleep 1
        elapsed=$((elapsed + 1))
        
        if [ $((elapsed % 15)) -eq 0 ]; then
            log_info "  Still waiting for $topic... (${elapsed}s elapsed)"
        fi
    done
    
    log_error "Timeout waiting for topic $topic"
    return 1
}

# Cleanup function to kill all child processes on exit
cleanup() {
    log_warn "Cleaning up processes..."
    
    # Kill ROS2 processes inside the container
    log_warn "Stopping ROS2 nodes in container..."
    docker exec "$CONTAINER" bash -c "pkill -f 'ros2 launch' || true" 2>/dev/null || true
    docker exec "$CONTAINER" bash -c "pkill -f 'python3.*launch' || true" 2>/dev/null || true
    docker exec "$CONTAINER" bash -c "pkill -f '_node' || true" 2>/dev/null || true
    
    # Give processes time to terminate gracefully
    sleep 2
    
    # Force kill if still running
    docker exec "$CONTAINER" bash -c "pkill -9 -f 'ros2' || true" 2>/dev/null || true
    
    # Kill local background processes (docker exec commands)
    pkill -P $$ || true
    
    log_info "Cleanup complete"
    exit 0
}

trap cleanup SIGINT SIGTERM

# Verify container is running
if ! docker ps --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
    log_error "Container '$CONTAINER' is not running!"
    log_error "Start it with: docker compose -f docker-compose.jetson.yml up -d"
    exit 1
fi

log_info "Starting ROS2 sequential launch in container '$CONTAINER'..."

# ============================================================================
# STAGE 1: Launch sensors
# ============================================================================
log_info "Stage 1: Launching sensors..."

docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash && ros2 launch sdk_wrap sensors_launch.py" &
SENSORS_PID=$!

log_info "Waiting for sensor topics to be publishing..."

# Topics from sensors_launch.py that should be publishing
SENSOR_TOPICS=(
    "/imu"
    "/odom_raw"
    "/scan"
    "/livox/points"
    "/cloud_in"
    "/tf"
)

TIMEOUT=120

for topic in "${SENSOR_TOPICS[@]}"; do
    if ! wait_for_topic "$topic" $TIMEOUT; then
        log_error "Failed to detect publishing on $topic"
        exit 1
    fi
done

log_info "✓ All sensor topics are publishing!"
sleep 2  # Give it a moment to stabilize

# ============================================================================
# STAGE 2: Launch SLAM
# ============================================================================
log_info "Stage 2: Launching SLAM toolbox..."

docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash && ros2 launch slam_toolbox online_async_launch.py slam_params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/slam_params.yaml" &
SLAM_PID=$!

log_info "Waiting for SLAM /map topic to be publishing..."

if ! wait_for_topic "/map" $TIMEOUT; then
    log_error "Failed to detect publishing on /map"
    exit 1
fi

log_info "✓ SLAM /map topic is publishing!"
sleep 2  # Give it a moment to stabilize

# ============================================================================
# STAGE 3: Launch Navigation
# ============================================================================
log_info "Stage 3: Launching Nav2..."

docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash && ros2 launch nav2_bringup navigation_launch.py params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/nav2_params.yaml map:=/root/limx_ws/map_saves/map_area_2.yaml" &
NAV_PID=$!

log_info "Waiting for Nav2 topics to be publishing..."

# Key Nav2 topics that indicate the navigation stack is ready
NAV2_TOPICS=(
    "/local_costmap/costmap"
    "/global_costmap/costmap"
)

for topic in "${NAV2_TOPICS[@]}"; do
    if ! wait_for_topic "$topic" $TIMEOUT; then
        log_error "Failed to detect publishing on $topic"
        exit 1
    fi
done

log_info "✓ Nav2 topics are publishing!"

# ============================================================================
# ALL DONE
# ============================================================================
echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}              ALL DONE                      ${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
log_info "All systems launched and publishing!"
log_info "ROS2 nodes running in container: $CONTAINER"
log_info "  - Sensors (PID: $SENSORS_PID)"
log_info "  - SLAM    (PID: $SLAM_PID)"
log_info "  - Nav2    (PID: $NAV_PID)"
log_info ""

# ============================================================================
# STAGE 4: Launch Out of Place Routine
# ============================================================================
log_info "Stage 4: Launching Out of Place Routine..."

docker exec "$CONTAINER" bash -c "unset RMW_IMPLEMENTATION && source /opt/ros/jazzy/setup.bash && source /root/limx_ws/install/setup.bash && python3 /root/limx_ws/src/scripts/out_of_place_routine.py"

log_info "Out of Place Routine completed!"
log_info ""
log_info "Press Ctrl+C to stop all processes"

# Wait for all background processes
wait
