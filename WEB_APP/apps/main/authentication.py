from rest_framework_simplejwt.authentication import JWTAuthentication
from django.core.exceptions import PermissionDenied
from .models import RevokedToken

class CustomJWTAuthentication(JWTAuthentication):
    def authenticate(self, request):
        auth_result = super().authenticate(request)
        if auth_result is not None:
            user, token = auth_result
            # Verificăm dacă tokenul este în baza de date ca revocat
            if RevokedToken.objects.filter(token=str(token)).exists():
                raise PermissionDenied("Acest token a fost revocat.")
        return auth_result
