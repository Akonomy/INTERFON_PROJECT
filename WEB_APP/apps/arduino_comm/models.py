
from django.db import models
from django.core.validators import MinValueValidator, MaxValueValidator
from dataclasses import dataclass, field
from bson.objectid import ObjectId
from django.utils import timezone






# Create your models here.
# models.py (only the DEVICE class shown; keep your other models unchanged)
import secrets
from django.db import models
from django.core.validators import MinValueValidator, MaxValueValidator

class DEVICE(models.Model):
    name = models.CharField(max_length=100, unique=True, help_text="A unique name for the device, e.g., 'Main_Entrance_ESP32'")
    ip_address = models.GenericIPAddressField(protocol='IPv4', blank=True, null=True)
    last_heartbeat = models.DateTimeField(auto_now=True)
    is_active = models.BooleanField(default=False)
    wifi_signal_strength = models.IntegerField(
        validators=[MinValueValidator(-100), MaxValueValidator(0)],
        blank=True,
        null=True
    )

    api_key = models.CharField(max_length=128, blank=True, null=True, unique=True, help_text="Secret key for device authentication")

    custom_field_1 = models.CharField(max_length=255, blank=True, null=True)
    custom_field_2 = models.CharField(max_length=255, blank=True, null=True)
    custom_field_3 = models.CharField(max_length=255, blank=True, null=True)
    custom_field_4 = models.CharField(max_length=255, blank=True, null=True)
    custom_field_5 = models.CharField(max_length=255, blank=True, null=True)

    def ensure_key(self):
        if not self.api_key:
            self.api_key = secrets.token_hex(32)  # 64 hex chars
            self.save()

    def __str__(self):
        return f"{self.name} ({'active' if self.is_active else 'disabled'})"

    class Meta:
        verbose_name = "DEVICE"
        verbose_name_plural = "DEVICES"


class SENSOR(models.Model):
    class SensorType(models.TextChoices):
        READ_ONLY = "readonly", "Read-Only"
        SETTABLE = "settable", "Settable"

    device = models.ForeignKey("DEVICE", on_delete=models.CASCADE, related_name="sensors")

    id_sensor = models.CharField(
        max_length=64,
        default="UNKNOWN_SENSOR",
        help_text="Unique sensor ID reported by device",
    )

    type = models.CharField(
        max_length=16,
        choices=SensorType.choices,
        default=SensorType.READ_ONLY,
        help_text="Whether sensor is passive (readonly) or can be written to",
    )

    name = models.CharField(
        max_length=100,
        help_text="Human-readable sensor name",
    )

    number = models.PositiveIntegerField(
        blank=True,
        null=True,
        help_text="Optional pin number or internal reference",
    )

    status = models.CharField(
        max_length=50,
        default="OFFLINE",
        help_text="Sensor health/status, e.g., OK, OFFLINE, ERROR",
    )

    value_int = models.FloatField(
        default=-99999,
        help_text="Primary numeric value (e.g., temperature, state)",
    )

    value_text = models.CharField(
        max_length=255,
        blank=True,
        null=True,
        help_text="Optional textual info (e.g., 'Door open', 'Charging')",
    )

    last_updated = models.DateTimeField(
        auto_now=True,
        help_text="When the server last received an update",
    )

    class Meta:
        verbose_name = "Sensor"
        verbose_name_plural = "Sensors"
        unique_together = ("device", "id_sensor")

    def save(self, *args, **kwargs):
        if not self.name:
            self.name = f"SENSOR_{self.id_sensor}"
        super().save(*args, **kwargs)

    def __str__(self):
        return f"{self.name} ({self.device.name}) → {self.value_int}"



class PERSONAL(models.Model):
    """
    Represents a person who can be assigned an access TAG.
    """
    full_name = models.CharField(max_length=200)
    email = models.EmailField(max_length=254, blank=True, null=True)
    created_at = models.DateTimeField(auto_now_add=True)
    notes = models.TextField(blank=True, null=True)

    def __str__(self):
        return self.full_name

    class Meta:
        verbose_name = "PERSONAL"
        verbose_name_plural = "PERSONAL"


class TAG(models.Model):
    """
    Represents an access tag (e.g., RFID, NFC) assigned to a person.
    """
    uid = models.CharField(max_length=100, unique=True, help_text="The unique identifier from the physical tag.")
    owner = models.ForeignKey(PERSONAL, on_delete=models.SET_NULL, null=True, blank=True, related_name='tags')
    is_allowed = models.BooleanField(default=False, help_text="Check this box if the tag is currently allowed access.")
    description = models.CharField(max_length=255, blank=True, null=True, help_text="Optional description, e.g., 'Main keychain fob'.")
    created_at = models.DateTimeField(auto_now_add=True)
    last_used = models.DateTimeField(null=True, blank=True)

    def __str__(self):
        owner_name = self.owner.full_name if self.owner else "Unassigned"
        return f"TAG UID: {self.uid} (Owner: {owner_name}, Allowed: {self.is_allowed})"

    class Meta:
        verbose_name = "TAG"
        verbose_name_plural = "TAGS"




# models.py

class TagRegisterRequest(models.Model):
    """
    Represents a pending request to register (add) a new tag.
    Origin: ESP32 device or Web user.
    """
    tag_uid = models.CharField(max_length=100, help_text="Raw UID from RFID/NFC tag")
    device = models.ForeignKey("DEVICE", on_delete=models.SET_NULL, null=True, blank=True)
    requested_at = models.DateTimeField(auto_now_add=True)
    requested_by = models.CharField(max_length=100, blank=True, null=True, help_text="Optional source info (e.g., ESP32, user)")
    status = models.CharField(
        max_length=20,
        choices=[
            ("pending", "Pending"),
            ("approved", "Approved"),
            ("rejected", "Rejected"),
        ],
        default="pending"
    )
    associated_person = models.ForeignKey("PERSONAL", on_delete=models.SET_NULL, null=True, blank=True)
    notes = models.TextField(blank=True, null=True)

    def __str__(self):
        return f"RegisterRequest: {self.tag_uid} ({self.status})"

    class Meta:
        verbose_name = "Tag Registration Request"
        verbose_name_plural = "Tag Registration Requests"
        ordering = ["-requested_at"]


class TagRevokeRequest(models.Model):
    """
    Represents a pending request to revoke access for an existing tag.
    """
    tag = models.ForeignKey("TAG", on_delete=models.CASCADE, related_name="revoke_requests")
    device = models.ForeignKey("DEVICE", on_delete=models.SET_NULL, null=True, blank=True)
    requested_at = models.DateTimeField(auto_now_add=True)
    requested_by = models.CharField(max_length=100, blank=True, null=True)
    reason = models.TextField(blank=True, null=True)
    status = models.CharField(
        max_length=20,
        choices=[
            ("pending", "Pending"),
            ("approved", "Approved"),
            ("rejected", "Rejected"),
        ],
        default="pending"
    )

    def __str__(self):
        return f"RevokeRequest: {self.tag.uid} ({self.status})"

    class Meta:
        verbose_name = "Tag Revoke Request"
        verbose_name_plural = "Tag Revoke Requests"
        ordering = ["-requested_at"]




class AccessLog(models.Model):
    class AccessResult(models.TextChoices):
        GRANTED = "granted", "Access Granted"
        DENIED = "denied", "Access Denied"
        ERROR = "error", "Unknown/Error"

    device = models.ForeignKey("DEVICE", on_delete=models.PROTECT, related_name="access_logs")
    tag = models.ForeignKey("TAG", on_delete=models.SET_NULL, null=True, blank=True, related_name="access_logs")
    person = models.ForeignKey("PERSONAL", on_delete=models.SET_NULL, null=True, blank=True)

    tag_uid = models.CharField(
    max_length=100,
    default="UNKNOWN",
    help_text="Raw UID of the tag used"
    )
    esp_timestamp = models.CharField(max_length=64, blank=True, null=True, help_text="ESP32 local timestamp (as string)")
    server_timestamp = models.DateTimeField(auto_now_add=True)

    result = models.CharField(
        max_length=16,
        choices=AccessResult.choices,
        default=AccessResult.ERROR,
    )

    details = models.CharField(max_length=255, blank=True, help_text="Details like 'Tag not assigned', 'Access granted to John'")

    def __str__(self):
        who = self.person.full_name if self.person else "Unknown"
        return f"[{self.server_timestamp:%Y-%m-%d %H:%M:%S}] {who} @ {self.device.name} → {self.get_result_display()}"

    class Meta:
        verbose_name = "Access Log"
        verbose_name_plural = "Access Logs"
        ordering = ["-server_timestamp"]





class Command(models.Model):
    """Concrete command containing the payload that must be executed by a device."""

    PARAM_VALIDATORS = [MinValueValidator(0), MaxValueValidator(65535)]

    device = models.ForeignKey(
        DEVICE,
        on_delete=models.CASCADE,
        related_name="commands",
        help_text="Device that should receive this command",
    )
    code = models.PositiveIntegerField(help_text="Numeric command code understood by the ESP32 firmware.")
    param1 = models.PositiveIntegerField(blank=True, null=True, validators=PARAM_VALIDATORS)
    param2 = models.PositiveIntegerField(blank=True, null=True, validators=PARAM_VALIDATORS)
    param3 = models.PositiveIntegerField(blank=True, null=True, validators=PARAM_VALIDATORS)
    param4 = models.PositiveIntegerField(blank=True, null=True, validators=PARAM_VALIDATORS)
    note = models.CharField(max_length=255, blank=True, help_text="Optional human readable description of the command.")
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ["-created_at"]

    def __str__(self):
        params = ",".join(
            str(p)
            for p in [self.param1, self.param2, self.param3, self.param4]
            if p is not None
        )
        params_display = f" [{params}]" if params else ""
        return f"{self.device.name} :: {self.code}{params_display}"

    @property
    def is_emergency(self) -> bool:
        return self.code >= 9000

    def as_uint16_tuple(self):
        """Return the command encoded as a tuple of five uint16 values."""

        def _sanitize(value):
            return int(value) if value is not None else 0

        return (
            _sanitize(self.code),
            _sanitize(self.param1),
            _sanitize(self.param2),
            _sanitize(self.param3),
            _sanitize(self.param4),
        )


class CommandQueueQuerySet(models.QuerySet):
    ACTIVE_STATUSES = ("queued", "delivered")

    def active(self):
        return self.filter(status__in=self.ACTIVE_STATUSES)

    def for_device(self, device):
        return self.filter(device=device)


class CommandQueue(models.Model):
    """Queue entry that determines when and how a command is delivered to a device."""

    class Status(models.TextChoices):
        QUEUED = "queued", "Queued"
        DELIVERED = "delivered", "Delivered"
        ACKNOWLEDGED = "acknowledged", "Acknowledged"
        EXPIRED = "expired", "Expired"
        CANCELLED = "cancelled", "Cancelled"

    command = models.OneToOneField(
        Command,
        on_delete=models.CASCADE,
        related_name="queue_entry",
    )
    device = models.ForeignKey(
        DEVICE,
        on_delete=models.CASCADE,
        related_name="command_queue",
    )
    is_emergency = models.BooleanField(default=False)
    status = models.CharField(
        max_length=20,
        choices=Status.choices,
        default=Status.QUEUED,
    )
    expires_at = models.DateTimeField(help_text="When the command should automatically expire.")
    enqueued_at = models.DateTimeField(auto_now_add=True)
    delivered_at = models.DateTimeField(blank=True, null=True)
    acknowledged_at = models.DateTimeField(blank=True, null=True)
    metadata = models.JSONField(blank=True, null=True)

    objects = CommandQueueQuerySet.as_manager()

    class Meta:
        ordering = ["-is_emergency", "enqueued_at"]

    def save(self, *args, **kwargs):
        if self.command and self.is_emergency is False:
            self.is_emergency = self.command.is_emergency
        if self.device_id is None and self.command_id:
            self.device = self.command.device
        super().save(*args, **kwargs)

    def mark_delivered(self):
        if self.status != self.Status.DELIVERED:
            self.status = self.Status.DELIVERED
            self.delivered_at = timezone.now()
            self.save(update_fields=["status", "delivered_at"])

    def acknowledge(self):
        self.status = self.Status.ACKNOWLEDGED
        now = timezone.now()
        self.acknowledged_at = now
        if not self.delivered_at:
            self.delivered_at = now
        self.save(update_fields=["status", "acknowledged_at", "delivered_at"])

    def mark_expired(self):
        if self.status not in {self.Status.ACKNOWLEDGED, self.Status.CANCELLED, self.Status.EXPIRED}:
            self.status = self.Status.EXPIRED
            self.acknowledged_at = timezone.now()
            self.save(update_fields=["status", "acknowledged_at"])
            CommandLog.objects.create(
                device=self.device,
                command=self.command,
                status=self.Status.EXPIRED,
                details="Command expired before being acknowledged.",
            )

    @property
    def is_expired(self) -> bool:
        return self.expires_at <= timezone.now()

    @classmethod
    def expire_overdue_for_device(cls, device):
        for entry in cls.objects.for_device(device).filter(
            status__in=[cls.Status.QUEUED, cls.Status.DELIVERED],
            expires_at__lt=timezone.now(),
        ).select_related("command"):
            entry.mark_expired()


class CommandLog(models.Model):
    """Audit trail of command execution lifecycle."""

    device = models.ForeignKey(DEVICE, on_delete=models.SET_NULL, null=True, blank=True)
    command = models.ForeignKey(Command, on_delete=models.SET_NULL, null=True, blank=True)
    status = models.CharField(max_length=32)
    details = models.TextField(blank=True)
    created_at = models.DateTimeField(auto_now_add=True)
    extra = models.JSONField(blank=True, null=True)

    class Meta:
        ordering = ["-created_at"]

    def __str__(self):
        device_name = self.device.name if self.device else "unknown"
        return f"[{self.created_at:%Y-%m-%d %H:%M:%S}] {device_name} -> {self.status}"



LOG_LEVELS = [
(0, 'EMERG'),
(1, 'ALERT'),
(2, 'CRIT'),
(3, 'ERR'),
(4, 'WARNING'),
(5, 'NOTICE'),
(6, 'INFO'),
(7, 'DEBUG'),
]


class SyslogEntry(models.Model):
    severity = models.IntegerField(default=6)
    timestamp = models.DateTimeField(auto_now_add=True)  # ← server time
    device_time = models.CharField(max_length=64, null=True, blank=True)  # ← ESP local time (optional)

    ip = models.GenericIPAddressField(null=True, blank=True)
    host = models.CharField(max_length=64)
    facility = models.CharField(max_length=64)
    priority = models.IntegerField(choices=LOG_LEVELS)
    tag = models.CharField(max_length=64)
    message = models.TextField()

    def get_priority_label(self):
        return dict(LOG_LEVELS).get(self.priority, 'UNKNOWN')





