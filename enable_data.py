import json
import uuid
import time
import websocket

ACCID = "WF_TRON1A_042"

# WebSocket client instance
ws_client = None

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
    print(f"Sending: {message_str}")
    
    # Send the message through WebSocket if client is connected
    if ws_client:
        ws_client.send(message_str)

# Track responses
responses_received = {"imu": False, "odom": False}

# WebSocket on_open callback
def on_open(ws):
    print("Connected to TRON1!")
    print("Enabling IMU...")
    send_request("request_enable_imu", {"enable": True})
    time.sleep(0.5)
    print("Enabling Odometry...")
    send_request("request_enable_odom", {"enable": True})

# WebSocket on_message callback
def on_message(ws, message):
    print(f"Received: {message}")
    
    try:
        response = json.loads(message)
        title = response.get("title", "")
        
        # Check for IMU response
        if "imu" in title.lower():
            responses_received["imu"] = True
            print("✓ IMU enabled successfully")
        
        # Check for odometry response
        if "odom" in title.lower():
            responses_received["odom"] = True
            print("✓ Odometry enabled successfully")
        
        # If both responses received, close connection
        if all(responses_received.values()):
            print("\nBoth IMU and Odometry enabled. Closing connection...")
            ws.close()
    except json.JSONDecodeError:
        pass

# WebSocket on_close callback
def on_close(ws, close_status_code, close_msg):
    print("Connection closed.")

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
    print("Connecting to TRON1 at ws://10.192.1.2:5000...")
    print("Press Ctrl+C to exit.")
    ws_client.run_forever()

if __name__ == "__main__":
    main()