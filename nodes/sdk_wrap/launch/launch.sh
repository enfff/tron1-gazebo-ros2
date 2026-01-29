#!/bin/bash

# Sequential ROS2 Launch Script with Log Monitoring
# This script launches three ROS2 processes sequentially, waiting for specific
# log messages before proceeding to the next launch.

set -e

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

# Cleanup function to kill all child processes on exit
cleanup() {
    log_warn "Cleaning up processes..."
    pkill -P $$ || true
    exit
}

trap cleanup SIGINT SIGTERM

# Temporary files for capturing output
TEMP_LOG_1=$(mktemp)
TEMP_LOG_2=$(mktemp)
TEMP_LOG_3=$(mktemp)

# Clean up temp files on exit
trap "rm -f $TEMP_LOG_1 $TEMP_LOG_2 $TEMP_LOG_3; cleanup" EXIT

log_info "Starting ROS2 sequential launch..."

# ============================================================================
# STAGE 1: Launch sensors
# ============================================================================
log_info "Stage 1: Launching sensors..."

ros2 launch sdk_wrap sensors_launch.py 2>&1 | tee $TEMP_LOG_1 &
SENSORS_PID=$!

log_info "Waiting for sensors to initialize (looking for 'Lidar[0] storage queue size')..."

# Monitor log file for the expected message
TIMEOUT=120  # 2 minutes timeout
ELAPSED=0
while [ $ELAPSED -lt $TIMEOUT ]; do
    if grep -q "Lidar\[0\] storage queue size" $TEMP_LOG_1 && \
       grep -q "livox/points publish use PointCloud2 format" $TEMP_LOG_1; then
        log_info "✓ Sensors initialized successfully!"
        sleep 2  # Give it a moment to stabilize
        break
    fi
    
    # Check if process died
    if ! kill -0 $SENSORS_PID 2>/dev/null; then
        log_error "Sensors process died unexpectedly!"
        exit 1
    fi
    
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ $ELAPSED -ge $TIMEOUT ]; then
    log_error "Timeout waiting for sensors to initialize!"
    exit 1
fi

# ============================================================================
# STAGE 2: Launch SLAM
# ============================================================================
log_info "Stage 2: Launching SLAM toolbox..."

ros2 launch slam_toolbox online_async_launch.py \
    slam_params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/slam_params.yaml \
    2>&1 | tee $TEMP_LOG_2 &
SLAM_PID=$!

log_info "Waiting for SLAM to initialize (looking for 'Registering sensor')..."

# Monitor log file for the expected message
TIMEOUT=120
ELAPSED=0
while [ $ELAPSED -lt $TIMEOUT ]; do
    if grep -q "Registering sensor.*Custom Described Lidar" $TEMP_LOG_2; then
        log_info "✓ SLAM initialized successfully!"
        sleep 3  # Give it more time to stabilize
        break
    fi
    
    # Check if process died
    if ! kill -0 $SLAM_PID 2>/dev/null; then
        log_error "SLAM process died unexpectedly!"
        exit 1
    fi
    
    # Debug: show last few lines
    if [ $((ELAPSED % 10)) -eq 0 ]; then
        log_info "Still waiting... (${ELAPSED}s elapsed)"
    fi
    
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ $ELAPSED -ge $TIMEOUT ]; then
    log_error "Timeout waiting for SLAM to initialize!"
    exit 1
fi

# ============================================================================
# STAGE 3: Launch Navigation
# ============================================================================
log_info "Stage 3: Launching Nav2..."

ros2 launch nav2_bringup navigation_launch.py \
    params_file:=/root/limx_ws/install/sdk_wrap/share/sdk_wrap/config/nav2_params.yaml \
    map:=/root/limx_ws/map_saves/map_area_2.yaml \
    2>&1 | tee $TEMP_LOG_3 &
NAV_PID=$!

log_info "✓ All systems launched!"
log_info "Processes running:"
log_info "  - Sensors (PID: $SENSORS_PID)"
log_info "  - SLAM    (PID: $SLAM_PID)"
log_info "  - Nav2    (PID: $NAV_PID)"
log_info ""
log_info "Press Ctrl+C to stop all processes"

# Wait for all processes
wait
