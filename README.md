# TRON-1 "Struzzo"

Graphical representation of the connections involved

``` mermaid
graph TD

    subgraph UGREEN["UGREEN"]
        LAN_IN["LAN IN"]
        LAN1_OUT["LAN1 OUT"]
        LAN2_OUT["LAN2 OUT"]
    end

    Jetson ---|LAN| LAN_IN
    LAN1_OUT ---|LAN| Laptop["Laptop"]
    LAN2_OUT ---|LAN| LIDAR["LiDAR"]
```

## Laptop

```bash
docker build -f Dockerfile.laptop -t tron1-sdk:laptop .
xhost +local:docker
docker compose -f docker-compose.laptop.yml up -d
docker exec -it tron1-sdk bash
```

After closing for the first time, you can execute the container interactively with

    docker exec -it tron1-sdk bash 

## Jetson

On your laptop:

```bash
ssh -X struzzo@struzzo
```

On the Jetson:

```bash
docker build -f Dockerfile.jetson -t struzzo:jetson .
echo $DISPLAY
export XAUTHORITY=$HOME/.Xauthority
docker compose -f docker-compose.jetson.yml up --build
```

## ROS 2 Data Bridge

The TRON1 high-level SDK provides sensor data (IMU, odometry, etc.) via WebSocket at `ws://10.192.1.2:5000`. For autonomous navigation with NAV2 and ROS2, this data needs to be bridged to ROS2 topics.

First, enable IMU and odometry data transmission using the `enable_data.py` script:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r requirements.txt
python3 enable_data.py
```

This sends WebSocket requests to enable the sensor data streams on the robot.

The nodes in the `nodes/` directory subscribe to the TRON1 low-level SDK and publish to standard ROS2 topics. See [nodes' README](nodes/README.md).