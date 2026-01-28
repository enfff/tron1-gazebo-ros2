#!/bin/bash
set -e

# Source ROS setup
source /opt/ros/jazzy/install/setup.bash
source /root/limx_ws/install/setup.bash

# Execute the command
exec "$@"
