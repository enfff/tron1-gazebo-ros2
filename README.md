# TRON-1 "Struzzo"

## Build Docker image

```bash
docker build -t struzzo:iron .
```

## Run with Docker Compose (GUI)

```bash
xhost +local:
docker compose up
```

Please uncomment the lines 12-18 if you're not using a NVIDIA gpu.