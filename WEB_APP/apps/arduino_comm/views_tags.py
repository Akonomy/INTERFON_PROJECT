# apps/arduino_comm/views_tags.py

from django.shortcuts import render, get_object_or_404, redirect
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.core.cache import cache
import json
from django.contrib import messages
from .models import TAG, TagRegisterRequest, TagRevokeRequest, DEVICE
from django import forms
from django.views.decorators.http import require_POST
from django.utils import timezone as dj_tz






class TagForm(forms.ModelForm):
    class Meta:
        model = TAG
        fields = ['uid', 'owner', 'is_allowed', 'description']





# ──────── Web Views ────────
def tag_list(request):
    tags = TAG.objects.select_related("owner").all()
    return render(request, "arduino_comm/TAGS/tag_list.html", {"tags": tags})


def tag_add(request):
    if request.method == "POST":
        form = TagForm(request.POST)
        if form.is_valid():
            form.save()
            messages.success(request, "New tag added successfully.")
            return redirect("arduino_comm:tag_list")
    else:
        form = TagForm()

    return render(request, "arduino_comm/TAGS/tag_add.html", {"form": form})


def tag_edit(request, tag_id):
    tag = get_object_or_404(TAG, pk=tag_id)
    if request.method == "POST":
        form = TagForm(request.POST, instance=tag)
        if form.is_valid():
            form.save()
            messages.success(request, f"Tag {tag.uid} updated successfully.")
            return redirect("arduino_comm:tag_list")
    else:
        form = TagForm(instance=tag)

    return render(request, "arduino_comm/TAGS/tag_edit.html", {"form": form, "tag": tag})
def tag_delete(request, tag_id):
    tag = get_object_or_404(TAG, pk=tag_id)
    tag.delete()
    return redirect("arduino_comm:tag_list")

def tag_allow(request, tag_id):
    tag = get_object_or_404(TAG, pk=tag_id)
    tag.is_allowed = True
    tag.save()
    return redirect("arduino_comm:tag_list")

def tag_revoke(request, tag_id):
    tag = get_object_or_404(TAG, pk=tag_id)
    tag.is_allowed = False
    tag.save()
    return redirect("arduino_comm:tag_list")

def tag_register_requests(request):
    requests = TagRegisterRequest.objects.order_by("-requested_at")[:100]
    return render(request, "arduino_comm/TAGS/tag_register_requests.html", {"requests": requests})

def tag_revoke_requests(request):
    requests = TagRevokeRequest.objects.select_related("tag").order_by("-requested_at")[:100]
    return render(request, "arduino_comm/TAGS/tag_revoke_requests.html", {"requests": requests})

# ──────── API Views ────────
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
    notes = data.get("notes", "")
    if not tag_uid:
        return JsonResponse({"error": "Missing tag_uid"}, status=400)

    req = TagRegisterRequest.objects.create(
        device_id=device_id,
        tag_uid=tag_uid,
        notes=notes,
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




def approve_tag_register_request(request, request_id):
    """Approve a pending tag registration request and create a TAG."""
    req = get_object_or_404(TagRegisterRequest, id=request_id)

    # Check if a tag with this UID already exists
    tag, created = TAG.objects.get_or_create(
        uid=req.tag_uid,
        defaults={
            "description": req.notes or "",
            "is_allowed": True,
        },
    )

    if not created:
        # If it already existed, just make sure it's allowed
        tag.is_allowed = True
        tag.save()

    req.status = "approved"
    req.save()

    messages.success(request, f"Tag {tag.uid} approved and added.")
    return redirect("arduino_comm:tag_register_requests")


def reject_tag_register_request(request, request_id):
    """Reject a pending tag registration request."""
    req = get_object_or_404(TagRegisterRequest, id=request_id)
    req.status = "rejected"
    req.save()

    messages.warning(request, f"Tag registration {req.tag_uid} rejected.")
    return redirect("arduino_comm:tag_register_requests")



def approve_tag_revoke_request(request, request_id):
    """Approve a revoke request and delete the associated tag."""
    req = get_object_or_404(TagRevokeRequest, id=request_id)

    tag = req.tag  # store a reference before deleting

    # Mark request as approved first
    req.status = "approved"
    req.tag = None  # remove FK reference
    req.save()

    # Now safely delete the tag
    if tag:
        tag_uid = tag.uid
        tag.delete()
        messages.success(request, f"Tag {tag_uid} revoked and deleted.")
    else:
        messages.warning(request, "No tag found for this revoke request.")

    return redirect("arduino_comm:tag_revoke_requests")


def reject_tag_revoke_request(request, request_id):
    """Reject a revoke request."""
    req = get_object_or_404(TagRevokeRequest, id=request_id)
    req.status = "rejected"
    req.save()

    messages.info(request, f"Revoke request for tag {req.tag.uid} rejected.")
    return redirect("arduino_comm:tag_revoke_requests")



@require_POST
def delete_all_register_requests_for_tag(request, request_id):
    """
    Delete ALL TagRegisterRequest entries that share the same tag_uid
    as the selected request.
    """
    req = get_object_or_404(TagRegisterRequest, id=request_id)

    tag_uid = req.tag_uid

    deleted_count, _ = TagRegisterRequest.objects.filter(tag_uid=tag_uid).delete()

    messages.success(
        request,
        f"Deleted {deleted_count} registration request(s) for tag {tag_uid}."
    )

    return redirect("arduino_comm:tag_register_requests")


@require_POST
def delete_all_revoke_requests_for_tag(request, request_id):
    """
    Delete ALL TagRevokeRequest entries for the same tag
    as the selected revoke request.
    """
    req = get_object_or_404(TagRevokeRequest, id=request_id)

    tag = req.tag
    tag_uid = tag.uid if tag else "Unknown"

    deleted_count, _ = TagRevokeRequest.objects.filter(tag=tag).delete()

    messages.success(
        request,
        f"Deleted {deleted_count} revoke request(s) for tag {tag_uid}."
    )

    return redirect("arduino_comm:tag_revoke_requests")



@csrf_exempt
def api_tag_status_request(request):
    """
    Lightweight approval status checker.
    Safe to poll frequently from device.
    Returns ONLY a small numeric status code.
    """

    if request.method != "POST":
        return JsonResponse({"error": "POST only"}, status=405)

    # validate session
    session_token = request.headers.get("X-Session-Token")
    session = cache.get(f"session:{session_token}")
    if not session:
        return JsonResponse({"error": "Unauthorized"}, status=401)

    # parse body
    try:
        data = json.loads(request.body.decode())
    except Exception:
        return JsonResponse({"error": "Invalid JSON"}, status=400)

    tag_uid = data.get("tag_uid")
    if not tag_uid:
        return JsonResponse({"error": "Missing tag_uid"}, status=400)

    # ───── 1) check registration workflow first ─────
    req = (
        TagRegisterRequest.objects
        .filter(tag_uid=tag_uid)
        .order_by("-requested_at")
        .first()
    )

    if req:
        if req.status == "pending":
            return JsonResponse({"status": 0})  # waiting approval

        if req.status == "rejected":
            return JsonResponse({"status": 3})  # rejected

    # ───── 2) check existing TAG ─────
    tag = TAG.objects.filter(uid=tag_uid).first()
    if not tag:
        # not registered yet → still pending
        return JsonResponse({"status": 0})

    # expired? (optional rule)
    age_seconds = (dj_tz.now() - tag.created_at).total_seconds()
    if age_seconds > 10 * 60:
        return JsonResponse({"status": 3})  # treat as rejected/invalid

    # revoked or not allowed
    if not tag.is_allowed:
        return JsonResponse({"status": 3})

    # approved but unassigned (valid, but your device may still proceed later)
    if tag.is_allowed and not tag.owner:
        return JsonResponse({"status": 1})

    # fully approved and assigned → now device should call full info API
    return JsonResponse({"status": 2})


