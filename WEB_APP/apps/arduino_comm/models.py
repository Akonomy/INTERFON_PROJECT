
from django.db import models
from django.core.validators import MinValueValidator, MaxValueValidator
from dataclasses import dataclass, field
from bson.objectid import ObjectId

@dataclass
class Sensor:
    """Representation of a Sensor document in MongoDB."""
    id: str = field(default_factory=lambda: str(ObjectId()))  # Use MongoDB ObjectId as string
    name: str = ""
    type: str = "input"  # "input" or "output"
    mode: str = "digital"  # "digital" or "analog"
    status: str = "off"  # "on" or "off"
    value: int = 0
    active: bool = False




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


class LED(models.Model):
    """
    Represents an LED connected to a device.
    """
    STATE_CHOICES = [
        (0, 'OFF'),
        (1, 'ON'),
    ]
    device = models.ForeignKey(DEVICE, on_delete=models.CASCADE, related_name='leds')
    name = models.CharField(max_length=100, help_text="Descriptive name, e.g., 'Status_Indicator_Green'")
    number = models.PositiveIntegerField(help_text="Pin number or identifier for the LED on the device.")
    state = models.IntegerField(choices=STATE_CHOICES, default=0, help_text="Current state of the LED (0=OFF, 1=ON).")
    is_active = models.BooleanField(default=True, help_text="Is this LED configuration currently active?")

    def __str__(self):
        return f"{self.name} on {self.device.name} is {self.get_state_display()}"

    class Meta:
        verbose_name = "LED"
        verbose_name_plural = "LEDs"
        # Ensures that each LED number is unique per device
        unique_together = ('device', 'number',)


class SENSOR(models.Model):
    """
    Represents a sensor connected to a device.
    """
    device = models.ForeignKey(DEVICE, on_delete=models.CASCADE, related_name='sensors')
    name = models.CharField(max_length=100, help_text="Descriptive name, e.g., 'Door_Magnetic_Switch' or 'Room_Temperature'")
    number = models.PositiveIntegerField(help_text="Pin number or identifier for the sensor on the device.")
    status = models.CharField(max_length=50, default="OK", help_text="Operational status of the sensor, e.g., 'OK', 'ERROR', 'OFFLINE'.")
    value = models.FloatField(blank=True, null=True, help_text="The last read value from the sensor.")
    last_updated = models.DateTimeField(auto_now=True)

    def __str__(self):
        return f"{self.name} on {self.device.name} | Value: {self.value}"

    class Meta:
        verbose_name = "SENSOR"
        verbose_name_plural = "SENSORS"
        # Ensures that each sensor number is unique per device
        unique_together = ('device', 'number',)


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


class AccessLog(models.Model):
    """
    Logs every access attempt made with a TAG on a DEVICE.
    """
    device = models.ForeignKey(DEVICE, on_delete=models.PROTECT, related_name='access_logs')
    tag = models.ForeignKey(TAG, on_delete=models.SET_NULL, null=True, blank=True, related_name='access_logs')
    timestamp = models.DateTimeField(auto_now_add=True)
    access_granted = models.BooleanField()
    details = models.CharField(max_length=255, blank=True, help_text="Additional details, e.g., 'Access Denied: Tag not allowed'.")

    def __str__(self):
        status = "Granted" if self.access_granted else "Denied"
        return f"Access {status} for tag {self.tag.uid if self.tag else 'N/A'} at {self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}"

    class Meta:
        verbose_name = "Access Log"
        verbose_name_plural = "Access Logs"
        ordering = ['-timestamp']


