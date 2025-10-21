# TRON-1 "Struzzo"

## Build

```bash
docker build -t struzzo:iron .
```

## Laptop (local development)

```bash
xhost +local:docker
docker compose -f docker-compose.laptop.yml up
```

## Jetson (deploy)

On your laptop:

```bash
ssh -X struzzo@struzzo
```

On the Jetson shell created above:

```bash
export XAUTHORITY=$HOME/.Xauthority
docker compose -f docker-compose.jetson.yml up
```