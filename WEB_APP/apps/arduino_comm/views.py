from django.shortcuts import render, get_object_or_404, redirect
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
import requests

from .models import LED, DEVICE



# views.py
import hmac
import hashlib
import json
import secrets
import base64
from datetime import timedelta

from django.views.decorators.csrf import csrf_exempt
from django.core.cache import cache
from django.http import JsonResponse
from django.utils import timezone as dj_tz

from .models import DEVICE, TAG

# Config (tweak as needed)
CHALLENGE_TTL = 60         # challenge expiry in seconds
SESSION_TTL = 120          # session token TTL in seconds
MIN_RESPONSE_DELAY = 2     # server indicates device should wait ~2s
GARBAGE_LEN = 64           # length for garbage hex on unknown keys


def compute_hmac_hex(key: str, message: str) -> str:
    return hmac.new(key.encode('utf-8'), message.encode('utf-8'), hashlib.sha256).hexdigest()


def random_garbage_hex(length=GARBAGE_LEN):
    return secrets.token_hex(length // 2)


@csrf_exempt
def request_challenge(request):
    """
    POST {"key": "<device_api_key>"}
    Response:
      if key known -> {"status":"ok","nonce":"base64","ts":<epoch>,"min_delay_seconds":2}
      if key unknown -> {"status":"garbage","nonce":"<random hex>"}
    """
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    try:
        data = json.loads(request.body.decode() or "{}")
    except Exception:
        data = request.POST.dict()

    key = data.get("key")
    if not key:
        return JsonResponse({"error": "Missing key"}, status=400)

    try:
        device = DEVICE.objects.get(api_key=key, is_active=True)
    except DEVICE.DoesNotExist:
        return JsonResponse({"status": "garbage", "nonce": random_garbage_hex()}, status=200)

    nonce_bytes = secrets.token_bytes(16)
    nonce_b64 = base64.b64encode(nonce_bytes).decode('ascii')
    ts = int(dj_tz.now().timestamp())

    cache_key = f"challenge:{device.id}:{nonce_b64}"
    cache.set(cache_key, {"device_id": device.id, "key": device.api_key, "ts": ts}, timeout=CHALLENGE_TTL)

    return JsonResponse({"status": "ok", "nonce": nonce_b64, "ts": ts, "min_delay_seconds": MIN_RESPONSE_DELAY}, status=200)


@csrf_exempt
def respond_challenge(request):
    """
    POST {
      "key": "<device_api_key>" OR "device_id": <id>,
      "nonce": "<base64 nonce>",
      "response": "<hex hmac>",
      "ts": <epoch>
    }
    On success -> {"status":"ok","session_token":"...","expires_in":SESSION_TTL}
    On fail -> {"status":"invalid","garbage":"..."}
    """
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    try:
        data = json.loads(request.body.decode() or "{}")
    except Exception:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    nonce_b64 = data.get("nonce")
    response_hex = data.get("response")
    key = data.get("key")
    device_id = data.get("device_id")

    if not nonce_b64 or not response_hex or not (key or device_id):
        return JsonResponse({"error": "Missing fields"}, status=400)

    # locate the cached challenge
    challenge = None
    if device_id:
        cache_key = f"challenge:{device_id}:{nonce_b64}"
        challenge = cache.get(cache_key)
    else:
        try:
            device = DEVICE.objects.get(api_key=key, is_active=True)
            cache_key = f"challenge:{device.id}:{nonce_b64}"
            challenge = cache.get(cache_key)
        except DEVICE.DoesNotExist:
            challenge = None

    if not challenge:
        return JsonResponse({"status": "invalid", "reason": "no_challenge", "garbage": random_garbage_hex()}, status=200)

    device_key = challenge["key"]
    challenge_ts = str(challenge["ts"])
    canonical = f"{nonce_b64}|{challenge_ts}"
    expected = compute_hmac_hex(device_key, canonical)

    if not hmac.compare_digest(expected, response_hex):
        cache.delete(cache_key)
        return JsonResponse({"status": "invalid", "reason": "bad_response", "garbage": random_garbage_hex()}, status=200)

    # success
    cache.delete(cache_key)
    session_token = secrets.token_hex(24)
    session_cache_key = f"session:{session_token}"
    cache.set(session_cache_key, {"device_id": challenge["device_id"]}, timeout=SESSION_TTL)

    return JsonResponse({"status": "ok", "session_token": session_token, "expires_in": SESSION_TTL}, status=200)


@csrf_exempt
def api_tag_check(request):
    """
    Protected API. Header: X-Session-Token: <token>
    Body: {"tag_uid": "..."}
    Returns {"access_granted": True|False, "owner": ...}
    """
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    session_token = request.headers.get("X-Session-Token") or request.META.get("HTTP_X_SESSION_TOKEN")
    if not session_token:
        return JsonResponse({"error": "Missing session token"}, status=401)

    session_info = cache.get(f"session:{session_token}")
    if not session_info:
        return JsonResponse({"error": "Invalid or expired session"}, status=401)

    try:
        data = json.loads(request.body.decode() or "{}")
    except Exception:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    tag_uid = data.get("tag_uid")
    if not tag_uid:
        return JsonResponse({"error": "Missing tag_uid"}, status=400)

    try:
        tag = TAG.objects.get(uid=tag_uid)
    except TAG.DoesNotExist:
        return JsonResponse({"access_granted": False, "reason": "unknown_tag"}, status=200)

    return JsonResponse({"access_granted": bool(tag.is_allowed), "owner": tag.owner.full_name if tag.owner else None}, status=200)







def led_control_page(request):
    """
    Displays all LEDs with controls.
    """
    leds = LED.objects.select_related('device').all()
    return render(request, 'arduino_comm/testpage.html', {'leds': leds})


@csrf_exempt
def toggle_led(request, led_id):
    """
    Toggle the LED state (ON/OFF) and send command to ESP32 device.
    """
    led = get_object_or_404(LED, id=led_id)
    device = led.device

    # Flip LED state
    new_state = 0 if led.state == 1 else 1
    led.state = new_state
    led.save()

    # Try to notify the ESP32 via HTTP
    if device.ip_address:
        try:
            url = f"http://{device.ip_address}/led"
            payload = {'number': led.number, 'state': new_state}
            response = requests.get(url, params=payload, timeout=3)
            response.raise_for_status()
        except Exception as e:
            return JsonResponse({
                'success': False,
                'message': f"ESP32 communication failed: {e}"
            })

    return JsonResponse({
        'success': True,
        'new_state': new_state,
        'message': f"LED {led.name} is now {'ON' if new_state else 'OFF'}"
    })


def register_led(request):
    """
    Simple form to add a new LED entry to the database.
    """
    if request.method == "POST":
        device_id = request.POST.get("device_id")
        name = request.POST.get("name")
        number = request.POST.get("number")
        device = get_object_or_404(DEVICE, id=device_id)
        LED.objects.create(device=device, name=name, number=number)
        return redirect('led_control_page')

    devices = DEVICE.objects.all()
    return render(request, 'arduino_comm/register_led.html', {'devices': devices})
