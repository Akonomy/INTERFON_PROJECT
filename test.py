import time
import hmac
import hashlib
import requests
import json
#from django.utils import timezone


# CONFIGURE THESE ↓↓↓
BASE_URL = "http://localhost:8000"
DEVICE_ID = 1
API_KEY = "f3109dd8c54f3ab0798c6b79756e404ed2ee350ee4b1cd9c2a7f4be40c59c57e"
INTERNAL_TOKEN = "your_internal_token_here"

def compute_signature(key, parts):
    message = "|".join(str(p) for p in parts)
    return hmac.new(key.encode(), message.encode(), hashlib.sha256).hexdigest()

def test_internal_enqueue():
    url = f"{BASE_URL}/api/internal/commands/enqueue/?token={INTERNAL_TOKEN}"
    data = {
        "device": DEVICE_ID,
        "code": 123,
        "params": [42],
        "note": "Automated test command",
        "expires_in": 120
    }
    print("→ Enqueueing command...")
    resp = requests.post(url, json=data)
    print(resp.status_code, resp.text)
    return resp.json().get("queue_id")

def test_public_poll():
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
    print(resp.status_code, resp.text)
    j = resp.json()
    if j.get("commands"):
        return j["commands"][0]["queue_id"]
    return None

def test_acknowledge(queue_id):
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
        "detail": "Command test ack"
    }
    print("→ Acknowledging command...")
    resp = requests.post(f"{BASE_URL}/api/commands/ack/", json=data)
    print(resp.status_code, resp.text)

if __name__ == "__main__":
    print("=== TEST START ===")
    queue_id = test_internal_enqueue()
    if not queue_id:
        print("✖ Failed to enqueue command. Cannot continue.")
        exit(1)
    time.sleep(1)  # Slight delay so queue is visible
    polled_id = test_public_poll()
    if not polled_id:
        print("✖ No command found during polling.")
    else:
        test_acknowledge(polled_id)
    print("=== TEST COMPLETE ===")
