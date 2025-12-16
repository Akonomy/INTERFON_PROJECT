# apps/accounts/decorators.py

from django.contrib.auth.decorators import user_passes_test

def role_required(required_role):
    def check(user):
        # Dacă utilizatorul este master, acordă acces indiferent de rolul specificat
        if user.is_master:
            return True
        # Pentru utilizatorii obișnuiți, verifică dacă rolul se potrivește
        return user.is_authenticated and user.role == required_role
    return user_passes_test(check)
