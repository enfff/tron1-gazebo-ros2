# TRON-1 "Struzzo"

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