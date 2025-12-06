from django.contrib.auth.models import AbstractUser
from djongo import models

class User(AbstractUser):
    _id = models.ObjectIdField(primary_key=True)
