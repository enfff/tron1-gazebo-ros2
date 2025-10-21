# Use ROS2 iron base image
FROM osrf/ros:iron-desktop

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ROBOT_TYPE=WF_TRON1A

# Install additional dependencies not in base image
RUN apt-get update && apt-get install -y \
    ros-iron-gazebo-ros-pkgs \
    ros-iron-gazebo-ros2-control \
    ros-iron-ros2-control \
    ros-iron-ros2-controllers \
    ros-iron-xacro \
    libmatio-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Create workspace directory
WORKDIR /root/limx_ws/src

# Clone required repositories
RUN git clone https://github.com/limxdynamics/robot-description.git && \
    git clone https://github.com/limxdynamics/limxsdk-lowlevel.git && \
    git clone https://github.com/limxdynamics/robot-visualization.git && \
    git clone https://github.com/limxdynamics/tron1-gazebo-ros2.git

# Build the workspace
WORKDIR /root/limx_ws
RUN /bin/bash -c "source /opt/ros/iron/setup.bash && \
    colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release"

# Set up entrypoint to source ROS2 and workspace
RUN echo '#!/bin/bash\n\
set -e\n\
source /opt/ros/iron/setup.bash\n\
source /root/limx_ws/install/setup.bash\n\
exec "$@"' > /ros_entrypoint.sh && \
    chmod +x /ros_entrypoint.sh

# --- Gazebo GUI quick reference ---
# Laptop without NVIDIA GPU:
#   xhost +local:docker
#   docker compose -f docker-compose.laptop.yml up
#
# Jetson (headless, streamed to laptop via ssh -X):
#   ssh -X <jetson-user>@<jetson-host>
#   export XAUTHORITY=$HOME/.Xauthority
#   docker compose -f docker-compose.jetson.yml up

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["bash"]
