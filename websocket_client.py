import json
import uuid
import threading
import time
import websocket  # provided by the 'websocket-client' package
from datetime import datetime

# Replace this ACCID value with your robot's actual serial number (SN)
ACCID = "WF_TRON1A_343"

# Atomic flag for graceful exit
should_exit = False

# WebSocket client instance
ws_client = None

# Twist publishing state (30 Hz)
twist_sending = False
twist_thread = None
twist_cmd = {"x": 0.0, "y": 0.0, "z": 0.0}

# Generate dynamic GUID
def generate_guid():
    return str(uuid.uuid4())

# Send WebSocket request with title and data
def send_request(title, data=None):
    if data is None:
        data = {}
    
    # Create message structure with necessary fields
    message = {
        "accid": ACCID,
        "title": title,
        "timestamp": int(time.time() * 1000),  # Current timestamp in milliseconds
        "guid": generate_guid(),
        "data": data
    }

    message_str = json.dumps(message)
    
    # Send the message through WebSocket if client is connected
    if ws_client:
        ws_client.send(message_str)


def twist_publisher_loop():
    """Continuously publish request_twist at 30 Hz while enabled."""
    global twist_sending
    print("[DEBUG] Twist publisher loop started!", flush=True)
    rate = 1.0 / 30.0
    next_time = time.time()
    count = 0
    while twist_sending and not should_exit:
        send_request("request_twist", twist_cmd)
        count += 1
        if count == 1:  # Print immediately on first send
            print(f"[DEBUG] First twist command sent: x={twist_cmd['x']}, y={twist_cmd['y']}, z={twist_cmd['z']}", flush=True)
        if count % 30 == 0:  # Print debug message every second (30 messages)
            print(f"[DEBUG] Sent {count} twist commands (x={twist_cmd['x']}, y={twist_cmd['y']}, z={twist_cmd['z']})", flush=True)
        next_time += rate
        sleep_duration = next_time - time.time()
        if sleep_duration > 0:
            time.sleep(sleep_duration)
        else:
            # If we're lagging, reset the schedule to now to avoid drift
            next_time = time.time()
    print(f"[DEBUG] Twist publisher loop stopped. Total commands sent: {count}", flush=True)

# Handle user commands
def handle_commands():
    global should_exit, twist_sending, twist_thread, twist_cmd
    while not should_exit:
        command = input("Enter command ('stand', 'walk', 'twist', 'sit', 'stair', 'stop', 'imu') or 'exit' to quit:\n")
        
        if command == "exit":
            twist_sending = False
            should_exit = True  # Set exit flag to stop the loop
            break
        elif command == "stand":
            send_request("request_stand_mode")  # Send stand mode request
        elif command == "walk":
            send_request("request_walk_mode")  # Send walk mode request
        elif command == "twist":
            # Get twist values from user and start 30 Hz publishing
            x = float(input("Enter x value:"))
            y = float(input("Enter y value:"))
            z = float(input("Enter z value:"))
            twist_cmd = {"x": x, "y": y, "z": z}
            print(f"[INFO] Setting twist values: x={x}, y={y}, z={z}", flush=True)
            if not twist_sending:
                twist_sending = True
                twist_thread = threading.Thread(target=twist_publisher_loop, daemon=True)
                twist_thread.start()
                print(f"[INFO] Started sending request_twist at 30 Hz", flush=True)
            else:
                print(f"[INFO] Stopped sending request_twist.", flush=True)
            print("Use 'twist' again to update values or 'twist_stop' to stop.", flush=True)
        elif command == "twist_stop":
            twist_sending = False
            print("Stopped sending request_twist.")
        elif command == "sit":
            send_request("request_sitdown")  # Send sit down request
        elif command == "stair":
            # Get stair mode enable flag from user
            enable = input("Enable stair mode (true/false):").strip().lower() == 'true'
            send_request("request_stair_mode", {"enable": enable})
        elif command == "stop":
            send_request("request_emgy_stop")  # Send emergency stop request
        elif command == "imu":
            # Get IMU enable flag from user
            enable = input("Enable IMU (true/false):").strip().lower() == 'true'
            send_request("request_enable_imu", {"enable": enable})

# WebSocket on_open callback
def on_open(ws):
    print("Connected!")
    # Start handling commands in a separate thread
    threading.Thread(target=handle_commands, daemon=True).start()

# WebSocket on_message callback
def on_message(ws, message):
    # Filter out battery status messages
    if "battery" in message.lower():
       return
    # Highlight twist-related messages
    if "twist" in message.lower():
        print(f"[TWIST RESPONSE] {message}", flush=True)
    else:
        print(f"Received message: {message}", flush=True)

# WebSocket on_close callback
def on_close(ws, close_status_code, close_msg):
    global twist_sending
    twist_sending = False
    print("Connection closed.")

# Close WebSocket connection
def close_connection(ws):
    ws.close()

def main():
    global ws_client
    
    # Create WebSocket client instance
    ws_client = websocket.WebSocketApp(
        "ws://10.192.1.2:5000",  # WebSocket server URI
        on_open=on_open,
        on_message=on_message,
        on_close=on_close
    )
    
    # Run WebSocket client loop
    print("Press Ctrl+C to exit.")
    ws_client.run_forever()

if __name__ == "__main__":
    main()