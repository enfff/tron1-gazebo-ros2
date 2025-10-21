# TRON-1 "Struzzo"

Graphical representation of the connections involved

``` mermaid
graph TD

    subgraph "LAN Network"
        Tron1["Tron 1"]
        Jetson["Jetson"]
        Tron1 --- Jetson
    end

    subgraph "Wi-Fi area42network"
        Router["Router"]
        Router --- Jetson
        Router --- Laptop1["Laptop 1"]
        Router --- Laptop2["Laptop 2"]
        Router --- Laptop3["Laptop 3"]
    end
```

## Laptop

```bash
docker build -t struzzo:iron .
xhost +local:docker
docker compose -f docker-compose.laptop.yml up
```

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