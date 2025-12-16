
from django.contrib.auth.models import AbstractUser
from django.db import models

class CustomUser(AbstractUser):
    is_master = models.BooleanField(default=False)  # True pentru utilizatorii master
    ROLE_CHOICES = (
        ('common', 'Common'),
        ('robot', 'Robot'),
        ('admin', 'Admin'),
    )
    role = models.CharField(max_length=20, choices=ROLE_CHOICES, blank=True, null=True)
    coins = models.IntegerField(default=0)

    def has_page_access(self, *page_permissions):
        """
        Returnează True dacă utilizatorul este master, superuser,
        sau dacă rolul utilizatorului se regăsește printre page_permissions.
        """
        if self.is_master or self.is_superuser:
            return True
        return self.role in page_permissions






class RevokedToken(models.Model):
    token = models.CharField(max_length=500, unique=True)
    revoked_at = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return self.token
