import time
import hmac
import hashlib
import requests
import json

# CONFIGURE THESE ↓↓↓
BASE_URL = "http://localhost:8000"
DEVICE_ID = 1
API_KEY = "f3109dd8c54f3ab0798c6b79756e404ed2ee350ee4b1cd9c2a7f4be40c59c57e"

def compute_signature(key, parts):
    message = "|".join(str(p) for p in parts)
    return hmac.new(key.encode(), message.encode(), hashlib.sha256).hexdigest()

def poll_command():
    ts = int(time.time())
    signature = compute_signature(API_KEY, [DEVICE_ID, ts])
    data = {
        "device": DEVICE_ID,
        "timestamp": ts,
        "signature": signature,
        "limit": 1
    }
    print("→ Polling for commands...")
    resp = requests.post(f"{BASE_URL}/api/commands/poll/", json=data)
    print(f"  HTTP {resp.status_code}")

    if resp.status_code != 200:
        print("✖ Poll failed.")
        return None

    j = resp.json()
    if j.get("commands"):
        return j["commands"][0]
    return None

def acknowledge_command(queue_id):
    ts = int(time.time())
    status = "acknowledged"
    parts = [DEVICE_ID, queue_id, status, ts]
    signature = compute_signature(API_KEY, parts)
    data = {
        "device": DEVICE_ID,
        "queue_id": queue_id,
        "timestamp": ts,
        "signature": signature,
        "status": status,
        "detail": "Automated acknowledgment"
    }
    resp = requests.post(f"{BASE_URL}/api/commands/ack/", json=data)
    print(f"→ Acknowledging command {queue_id}...")
    print(f"  HTTP {resp.status_code}: {resp.text.strip()}")

def format_command(cmd):
    code = cmd.get("code")
    params = cmd.get("params", [0, 0, 0, 0])
    expires = cmd.get("expires_at")
    queue_id = cmd.get("queue_id")
    emergency = cmd.get("emergency")

    print("\n📦 EXECUTING COMMAND")
    print("────────────────────────────")
    print(f"Queue ID     : {queue_id}")
    print(f"Code         : {code}")
    print(f"Params       : {params}")
    print(f"Expires At   : {expires}")
    print(f"Emergency    : {'🚨 YES' if emergency else 'no'}")
    print("────────────────────────────\n")

def run_command_loop():
    while True:
        cmd = poll_command()
        if not cmd:
            print("✅ No more commands in queue.")
            break

        format_command(cmd)

        queue_id = cmd.get("queue_id")
        if queue_id:
            acknowledge_command(queue_id)

        time.sleep(1)

if __name__ == "__main__":
    print("=== EXECUTING ALL QUEUED COMMANDS ===")
    run_command_loop()
    print("=== DONE ===")
