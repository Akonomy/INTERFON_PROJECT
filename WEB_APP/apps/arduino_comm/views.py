# ─────────────────────────────────────────────────────────────────────────────
# Standard Library Imports
# ─────────────────────────────────────────────────────────────────────────────
import base64
import hashlib
import hmac
import json
import logging
import secrets
import struct
from datetime import timedelta
from ipaddress import ip_address, ip_network

# ─────────────────────────────────────────────────────────────────────────────
# Django Core Imports
# ─────────────────────────────────────────────────────────────────────────────
from django.conf import settings
from django.core.cache import cache
from django.db import connection, transaction
from django.http import JsonResponse, HttpResponse
from django.shortcuts import render, get_object_or_404, redirect
from django.utils import timezone as dj_tz
from django.views.decorators.csrf import csrf_exempt
from django import forms
from django.urls import reverse
from django.contrib import messages
# ─────────────────────────────────────────────────────────────────────────────
# Third-Party Imports
# ─────────────────────────────────────────────────────────────────────────────
import requests


# ─────────────────────────────────────────────────────────────────────────────
# Local App Imports
# ─────────────────────────────────────────────────────────────────────────────
from .models import (
    DEVICE,
    TAG,
    SENSOR,
    PERSONAL,
    AccessLog,
    SyslogEntry,
    Command,
    CommandQueue,
    CommandLog,
)



# Config (tweak as needed)
CHALLENGE_TTL = 60         # challenge expiry in seconds
SESSION_TTL = 120          # session token TTL in seconds
MIN_RESPONSE_DELAY = 2     # server indicates device should wait ~2s
GARBAGE_LEN = 64           # length for garbage hex on unknown keys
PUBLIC_HMAC_TOLERANCE = getattr(settings, "ARDUINO_PUBLIC_HMAC_TOLERANCE", 60)
INTERNAL_TOKEN = getattr(settings, "ARDUINO_INTERNAL_TOKEN", None)
LOCAL_NETWORKS = getattr(
    settings,
    "ARDUINO_INTERNAL_NETWORKS",
    [
        "127.0.0.0/8",
        "10.0.0.0/8",
        "172.16.0.0/12",
        "192.168.0.0/16",
        "::1/128",
    ],
)

logger = logging.getLogger(__name__)




def compute_hmac_hex(key: str, message: str) -> str:
    return hmac.new(key.encode('utf-8'), message.encode('utf-8'), hashlib.sha256).hexdigest()


def random_garbage_hex(length=GARBAGE_LEN):
    return secrets.token_hex(length // 2)

def _parse_json_body(request):
    try:
        return json.loads(request.body.decode() or "{}")
    except (json.JSONDecodeError, UnicodeDecodeError):
        return {}


def _get_device(device_identifier):
    if device_identifier is None:
        return None
    lookup = {"id": device_identifier}
    if isinstance(device_identifier, str) and not device_identifier.isdigit():
        lookup = {"name": device_identifier}
    try:
        return DEVICE.objects.get(**lookup)
    except DEVICE.DoesNotExist:
        return None


def _is_internal_request(request):
    token = request.headers.get("X-Internal-Token") or request.GET.get("token")
    if token and INTERNAL_TOKEN and hmac.compare_digest(token, INTERNAL_TOKEN):
        return True

    remote_addr = request.META.get("REMOTE_ADDR")
    if not remote_addr:
        return False

    try:
        client_ip = ip_address(remote_addr)
    except ValueError:
        return False

    for net in LOCAL_NETWORKS:
        try:
            if client_ip in ip_network(net, strict=False):
                return True
        except ValueError:
            continue
    return False


def _validate_public_signature(device, timestamp, signature, canonical_parts):
    if not device or not signature:
        return False, "invalid_device"

    if timestamp is None:
        return False, "missing_timestamp"

    try:
        timestamp = int(timestamp)
    except (TypeError, ValueError):
        return False, "invalid_timestamp"

    now = int(dj_tz.now().timestamp())
    if abs(now - timestamp) > PUBLIC_HMAC_TOLERANCE:
        return False, "timestamp_out_of_range"

    canonical = "|".join(str(p) for p in canonical_parts)
    expected = hmac.new(device.api_key.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(expected, signature):
        return False, "signature_mismatch"
    return True, None


def _command_to_json(queue_entry):
    command = queue_entry.command
    return {
        "queue_id": queue_entry.id,
        "code": command.code,
        "params": [
            command.param1 or 0,
            command.param2 or 0,
            command.param3 or 0,
            command.param4 or 0,
        ],
        "expires_at": queue_entry.expires_at.isoformat(),
        "emergency": queue_entry.is_emergency,
    }


def _binary_payload_for_command(command):
    try:
        return struct.pack(">5H", *command.as_uint16_tuple())
    except struct.error as exc:
        raise ValueError(f"Command parameters out of range: {exc}") from exc




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
def public_poll_commands(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    data = _parse_json_body(request)
    device_identifier = data.get("device") or data.get("device_id")
    timestamp = data.get("timestamp")
    signature = data.get("signature")
    response_format = data.get("format", "json")
    try:
        limit_raw = data.get("limit", 1)
        limit = int(limit_raw)
    except (TypeError, ValueError):
        limit = 1
    limit = max(1, min(limit, 10))

    device = _get_device(device_identifier)
    if not device or not device.api_key:
        return JsonResponse({"status": "invalid", "reason": "unknown_device"}, status=401)

    ok, reason = _validate_public_signature(device, timestamp, signature, [device.id, timestamp])
    if not ok:
        return JsonResponse({"status": "invalid", "reason": reason}, status=401)

    CommandQueue.expire_overdue_for_device(device)

    with transaction.atomic():
        queue_qs = (
            CommandQueue.objects.select_related("command")
            .filter(device=device, status=CommandQueue.Status.QUEUED)
            .order_by("-is_emergency", "enqueued_at")
        )

        if connection.features.has_select_for_update:
            if connection.features.supports_select_for_update_with_skip_locked:
                queue_qs = queue_qs.select_for_update(skip_locked=True)
            else:
                queue_qs = queue_qs.select_for_update()

        queue_entries = list(queue_qs[:limit])

        if not queue_entries:
            return JsonResponse({"status": "ok", "commands": []}, status=200)

        payload = []
        for entry in queue_entries:
            entry.mark_delivered()
            payload.append(_command_to_json(entry))
            CommandLog.objects.create(
                device=device,
                command=entry.command,
                status="delivered",
                details="Command delivered to device.",
                extra={"queue_id": entry.id},
            )

    if response_format == "binary":
        if len(payload) != 1:
            return JsonResponse({"error": "Binary format supports a single command"}, status=400)
        command_entry = queue_entries[0]
        try:
            blob = _binary_payload_for_command(command_entry.command)
        except ValueError as exc:
            return JsonResponse({"error": str(exc)}, status=400)
        response = HttpResponse(blob, content_type="application/octet-stream")
        response["X-Queue-Id"] = str(command_entry.id)
        response["X-Command-Code"] = str(command_entry.command.code)
        return response

    return JsonResponse({"status": "ok", "commands": payload}, status=200)


@csrf_exempt
def public_acknowledge_command(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    data = _parse_json_body(request)
    queue_id = data.get("queue_id")
    device_identifier = data.get("device") or data.get("device_id")
    timestamp = data.get("timestamp")
    signature = data.get("signature")
    status_flag = data.get("status", "acknowledged")
    detail = data.get("detail", "")

    if queue_id is None:
        return JsonResponse({"status": "invalid", "reason": "missing_queue"}, status=400)

    device = _get_device(device_identifier)
    if not device or not device.api_key:
        return JsonResponse({"status": "invalid", "reason": "unknown_device"}, status=401)

    try:
        queue_id_int = int(queue_id)
    except (TypeError, ValueError):
        return JsonResponse({"status": "invalid", "reason": "queue_format"}, status=400)

    ok, reason = _validate_public_signature(device, timestamp, signature, [device.id, queue_id_int, status_flag, timestamp])
    if not ok:
        return JsonResponse({"status": "invalid", "reason": reason}, status=401)

    try:
        queue_entry = CommandQueue.objects.select_related("command").get(id=queue_id_int, device=device)
    except CommandQueue.DoesNotExist:
        return JsonResponse({"status": "invalid", "reason": "unknown_queue"}, status=404)

    if queue_entry.is_expired:
        queue_entry.mark_expired()
        return JsonResponse({"status": "expired"}, status=410)

    queue_entry.acknowledge()
    CommandLog.objects.create(
        device=device,
        command=queue_entry.command,
        status=status_flag,
        details=detail or "Command acknowledged by device.",
        extra={"queue_id": queue_entry.id},
    )
    return JsonResponse({"status": "ok"}, status=200)


@csrf_exempt
def internal_enqueue_command(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    if not _is_internal_request(request):
        return JsonResponse({"error": "forbidden"}, status=403)

    data = _parse_json_body(request)
    device_identifier = data.get("device") or data.get("device_id")
    code = data.get("code")
    params = data.get("params", [])
    note = data.get("note", "")

    if code is None:
        return JsonResponse({"error": "Missing code"}, status=400)

    device = _get_device(device_identifier)
    if not device:
        return JsonResponse({"error": "Unknown device"}, status=404)

    param_values = list(params) + [None] * (4 - len(params))
    param_values = param_values[:4]

    command_kwargs = {
        "device": device,
        "note": note,
    }
    for idx, value in enumerate(param_values, start=1):
        if value is not None:
            try:
                command_kwargs[f"param{idx}"] = int(value)
            except (TypeError, ValueError):
                return JsonResponse({"error": f"Invalid param{idx}"}, status=400)

    try:
        command_kwargs["code"] = int(code)
    except (TypeError, ValueError):
        return JsonResponse({"error": "Invalid code"}, status=400)

    try:
        expires_in_raw = data.get("expires_in", 60)
        expires_in = int(expires_in_raw)
    except (TypeError, ValueError):
        return JsonResponse({"error": "Invalid expires_in"}, status=400)

    command = Command.objects.create(**command_kwargs)
    expires_at = dj_tz.now() + timedelta(seconds=max(1, expires_in))

    queue_entry = CommandQueue.objects.create(
        command=command,
        device=device,
        expires_at=expires_at,
        metadata={"source": data.get("source", "internal_api")},
    )

    CommandLog.objects.create(
        device=device,
        command=command,
        status="queued",
        details="Command enqueued via internal API.",
        extra={"queue_id": queue_entry.id},
    )

    return JsonResponse({
        "status": "ok",
        "queue_id": queue_entry.id,
        "emergency": queue_entry.is_emergency,
        "expires_at": expires_at.isoformat(),
    })





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




# views_tag_requests.py (extending main view)

from django.shortcuts import render, get_object_or_404, redirect
from django.http import JsonResponse, HttpResponseNotAllowed
from django.views.decorators.csrf import csrf_exempt
from django import forms
from django.utils import timezone as dj_tz
import json
from .models import TAG, PERSONAL, TagRegisterRequest, TagRevokeRequest

# ──────────────── API VIEWS (ESP32/DEVICE) ────────────────
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





@csrf_exempt
def api_register_tag_request(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    session_token = request.headers.get("X-Session-Token")
    device_id = cache.get(f"session:{session_token}", {}).get("device_id")
    if not device_id:
        return JsonResponse({"error": "Unauthorized"}, status=401)

    try:
        data = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    tag_uid = data.get("tag_uid")
    description = data.get("description", "")
    if not tag_uid:
        return JsonResponse({"error": "Missing tag_uid"}, status=400)

    req = TagRegisterRequest.objects.create(
        device_id=device_id,
        tag_uid=tag_uid,
        description=description,
        status="pending"
    )
    return JsonResponse({"status": "ok", "request_id": req.id})


@csrf_exempt
def api_revoke_tag_request(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    session_token = request.headers.get("X-Session-Token")
    device_id = cache.get(f"session:{session_token}", {}).get("device_id")
    if not device_id:
        return JsonResponse({"error": "Unauthorized"}, status=401)

    try:
        data = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    tag_uid = data.get("tag_uid")
    reason = data.get("reason", "")
    if not tag_uid:
        return JsonResponse({"error": "Missing tag_uid"}, status=400)

    tag = TAG.objects.filter(uid=tag_uid).first()
    if not tag:
        return JsonResponse({"error": "Tag not found"}, status=404)

    req = TagRevokeRequest.objects.create(
        device_id=device_id,
        tag=tag,
        reason=reason,
        status="pending"
    )
    return JsonResponse({"status": "ok", "request_id": req.id})

# ──────────────── WEB VIEWS ────────────────

class PersonalForm(forms.ModelForm):
    class Meta:
        model = PERSONAL
        fields = ['full_name', 'email', 'notes']


class TagForm(forms.ModelForm):
    class Meta:
        model = TAG
        fields = ['uid', 'owner', 'is_allowed', 'description']


def tag_register_requests(request):
    requests = TagRegisterRequest.objects.select_related("tag").order_by("-created_at")[:100]
    return render(request, "arduino_comm/TAGS/tag_register_requests.html", {"requests": requests})



def tag_revoke_requests(request):
    requests = TagRevokeRequest.objects.select_related("tag").order_by("-created_at")[:100]
    return render(request, "arduino_comm/TAGS/tag_revoke_requests.html", {"requests": requests})

def personal_list(request):
    people = PERSONAL.objects.all()
    return render(request, "arduino_comm/TAGS/personal_list.html", {"people": people})


def personal_add(request):
    if request.method == "POST":
        form = PersonalForm(request.POST)
        if form.is_valid():
            form.save()
            return redirect("personal_list")
    else:
        form = PersonalForm()
    return render(request, "arduino_comm/TAGS/personal_form.html", {"form": form})


def personal_edit(request, person_id):
    person = get_object_or_404(PERSONAL, id=person_id)
    if request.method == "POST":
        form = PersonalForm(request.POST, instance=person)
        if form.is_valid():
            form.save()
            return redirect("personal_list")
    else:
        form = PersonalForm(instance=person)
    return render(request, "arduino_comm/TAGS/personal_form.html", {"form": form})


def personal_delete(request, person_id):
    person = get_object_or_404(PERSONAL, id=person_id)
    if request.method == "POST":
        person.delete()
        return redirect("personal_list")
    return HttpResponseNotAllowed(["POST"])


def tag_list(request):
    tags = TAG.objects.select_related("owner").all()
    return render(request, "arduino_comm/TAGS/tag_list.html", {"tags": tags})


def tag_add(request):
    if request.method == "POST":
        form = TagForm(request.POST)
        if form.is_valid():
            form.save()
            return redirect("tag_list")
    else:
        form = TagForm()
    return render(request, "arduino_comm/TAGS/tag_form.html", {"form": form})


def tag_edit(request, tag_id):
    tag = get_object_or_404(TAG, id=tag_id)
    if request.method == "POST":
        form = TagForm(request.POST, instance=tag)
        if form.is_valid():
            form.save()
            return redirect("tag_list")
    else:
        form = TagForm(instance=tag)
    return render(request, "arduino_comm/TAGS/tag_form.html", {"form": form})


def tag_delete(request, tag_id):
    tag = get_object_or_404(TAG, id=tag_id)
    if request.method == "POST":
        tag.delete()
        return redirect("tag_list")
    return HttpResponseNotAllowed(["POST"])


def tag_allow(request, tag_id):
    tag = get_object_or_404(TAG, id=tag_id)
    tag.is_allowed = True
    tag.save()
    return redirect("tag_list")


def tag_revoke(request, tag_id):
    tag = get_object_or_404(TAG, id=tag_id)
    tag.is_allowed = False
    tag.save()
    return redirect("tag_list")





class SendCommandForm(forms.Form):
    device = forms.ModelChoiceField(queryset=DEVICE.objects.filter(is_active=True), label="Device")
    code = forms.IntegerField(label="Command Code", min_value=0)
    param1 = forms.IntegerField(label="Param 1", required=False)
    param2 = forms.IntegerField(label="Param 2", required=False)
    param3 = forms.IntegerField(label="Param 3", required=False)
    param4 = forms.IntegerField(label="Param 4", required=False)
    expires_in = forms.IntegerField(label="Expires in (s)", initial=60, min_value=1)

from django.contrib import messages
from django.test import RequestFactory
import json

def web_send_command(request):
    if request.method == "POST":
        form = SendCommandForm(request.POST)
        if form.is_valid():
            device = form.cleaned_data['device']
            code = form.cleaned_data['code']
            params = [
                form.cleaned_data.get('param1') or 0,
                form.cleaned_data.get('param2') or 0,
                form.cleaned_data.get('param3') or 0,
                form.cleaned_data.get('param4') or 0,
            ]
            expires_in = form.cleaned_data['expires_in']

            # Construim payload pentru internal API
            payload = {
                "device": device.id,
                "code": code,
                "params": params,
                "expires_in": expires_in,
                "note": f"Web send by user {request.user.username}"
                if request.user.is_authenticated else
                "Web send anonymous",
            }

            # Simulare request intern
            rf = RequestFactory()
            fake_req = rf.post(
                "/fake-internal/",
                data=json.dumps(payload),
                content_type="application/json"
            )
            fake_req.META['REMOTE_ADDR'] = '127.0.0.1'

            resp = internal_enqueue_command(fake_req)

            result = json.loads(resp.content.decode())

            if resp.status_code == 200 and result.get("status") == "ok":
                queue_id = result.get("queue_id")
                messages.success(request, f"Command queued successfully (ID: {queue_id})")
            else:
                messages.error(request, f"Failed: {result}")



    else:
        form = SendCommandForm()

    return render(request, 'arduino_comm/send_command.html', {'form': form})







def syslog_view(request):
    logs = SyslogEntry.objects.order_by('-timestamp')[:100]
    return render(request, 'arduino_comm/syslog.html', {'logs': logs})






@csrf_exempt
def syslog_api_receiver(request):
    if request.method == "POST":
        try:
            data = json.loads(request.body)

            log = SyslogEntry.objects.create(
                severity=data.get("severity", 6),
                facility=data.get("facility", "user"),
                host=data.get("host", "unknown"),
                tag=data.get("tag", "esp32"),
                message=data.get("message", "No message"),
                ip=request.META.get("REMOTE_ADDR"),
                priority=data.get("priority", 14),
                device_time=data.get("device_time"),  # ⬅ added here
            )

            return JsonResponse({"status": "ok", "id": log.id})
        except Exception as e:
            return JsonResponse({"status": "error", "detail": str(e)}, status=400)

    return JsonResponse({"error": "Only POST allowed."}, status=405)


# views.py
@csrf_exempt
def device_status_update(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    session_token = request.headers.get("X-Session-Token")
    session_info = cache.get(f"session:{session_token}")
    if not session_info:
        return JsonResponse({"error": "Invalid or expired session"}, status=401)

    try:
        data = json.loads(request.body.decode())
    except json.JSONDecodeError:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    device = DEVICE.objects.get(id=session_info["device_id"])
    now = dj_tz.now()

    # Update device info
    device.last_heartbeat = now
    device.wifi_signal_strength = data.get("wifi_rssi", device.wifi_signal_strength)
    device.ip_address = request.META.get("REMOTE_ADDR")
    device.custom_field_1 = data.get("battery_status")
    device.custom_field_2 = data.get("esp32_time")
    device.save()

    # Optional: update sensors
    for sensor_data in data.get("sensors", []):
        SENSOR.objects.update_or_create(
            device=device,
            number=sensor_data.get("number"),
            defaults={
                "name": sensor_data.get("name", f"Sensor {sensor_data.get('number')}"),
                "value": sensor_data.get("value"),
                "status": sensor_data.get("status", "OK"),
            }
        )

    return JsonResponse({"status": "ok", "updated_at": now.isoformat()})



def sensor_list(request):
    sensors = SENSOR.objects.select_related("device").all()
    return render(request, "arduino_comm/sensor_list.html", {"sensors": sensors})




def sensor_edit(request, sensor_id):
    sensor = get_object_or_404(SENSOR, id=sensor_id)


    if request.method == "POST":
        sensor.name = request.POST.get("name", sensor.name)
        sensor.status = request.POST.get("status", sensor.status)
        sensor.value_int = request.POST.get("value_int", sensor.value_int)
        sensor.value_text = request.POST.get("value_text") or None
        sensor.save()
        return redirect("sensor_list")


    return render(request, "arduino_comm/sensor_edit.html", {"sensor": sensor})




def sensor_delete(request, sensor_id):
    sensor = get_object_or_404(SENSOR, id=sensor_id)
    if request.method == "POST":
        sensor.delete()
        return redirect("arduino_comm/sensor_list")
    return HttpResponseNotAllowed(["POST"])




class SensorForm(forms.ModelForm):
    class Meta:
        model = SENSOR
        fields = [
            'device', 'id_sensor', 'type', 'name', 'number',
            'status', 'value_int', 'value_text'
        ]


def sensor_add(request):
    if request.method == 'POST':
        form = SensorForm(request.POST)
        if form.is_valid():
            form.save()
            return redirect('sensor_list')
    else:
        form = SensorForm()

    return render(request, 'arduino_comm/sensor_add.html', {'form': form})



# ──────────────── ACCESS LOG VIEWS ────────────────
def access_log_list(request):
    logs = AccessLog.objects.select_related("device", "person", "tag").all()
    return render(request, "arduino_comm/access_log.html", {"logs": logs})



@csrf_exempt
def api_update_sensor(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    try:
        data = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    device_id = data.get("device_id")
    id_sensor = data.get("id_sensor")
    if not device_id or not id_sensor:
        return JsonResponse({"error": "Missing device_id or id_sensor"}, status=400)

    try:
        device = DEVICE.objects.get(name=device_id)  # or use .get(id=device_id)
    except DEVICE.DoesNotExist:
        return JsonResponse({"error": "Unknown device"}, status=404)

    sensor, _ = SENSOR.objects.get_or_create(device=device, id_sensor=id_sensor)

    sensor.status = data.get("status", sensor.status)
    sensor.value_int = data.get("value_int", sensor.value_int)
    sensor.value_text = data.get("value_text", sensor.value_text)
    sensor.save()

    return JsonResponse({"status": "ok", "sensor_id": sensor.id_sensor})


@csrf_exempt
def api_log_access(request):
    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    try:
        data = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    device_id = data.get("device_id")
    tag_uid = data.get("tag_uid")

    if not device_id or not tag_uid:
        return JsonResponse({"error": "Missing device_id or tag_uid"}, status=400)

    try:
        device = DEVICE.objects.get(name=device_id)
    except DEVICE.DoesNotExist:
        return JsonResponse({"error": "Unknown device"}, status=404)

    tag = TAG.objects.filter(uid=tag_uid).first()
    person = tag.owner if tag and tag.owner else None

    log = AccessLog.objects.create(
        device=device,
        tag=tag,
        person=person,
        tag_uid=tag_uid,
        esp_timestamp=data.get("esp_timestamp"),
        result=data.get("result", "error"),
        details=data.get("details", "")
    )

    return JsonResponse({"status": "ok", "access_log_id": log.id})
