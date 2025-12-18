from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse, HttpResponse
from django.contrib import messages
from django.contrib.auth import get_user_model



from apps.arduino_comm.models import SENSOR, SensorHistory




User = get_user_model()



# Custom error viewss
def custom_403_view(request, exception=None):
    return render(request, '403.html', status=403)

def custom_404_view(request, exception=None):
    return render(request, '404.html', status=404)

def custom_500_view(request):
    return render(request, '500.html', status=500)


import random

def dashboard_view(request):
    try:
        sensor = SENSOR.objects.get(id_sensor="BATERIE")

        history = sensor.history.order_by('timestamp')

        labels = [h.timestamp.strftime("%Y-%m-%d %H:%M") for h in history]
        values = [h.value_int for h in history]

    except SENSOR.DoesNotExist:
        # MOCK DATA BECAUSE PRODUCTION IS A DESERT
        now = timezone.now()

        labels = [
            (now - timedelta(minutes=5 * i)).strftime("%Y-%m-%d %H:%M")
            for i in range(10)
        ][::-1]

        values = [random.randint(20, 100) for _ in range(10)]

        sensor = {
            "id_sensor": "BATERIE (mock)",
            "status": "mocked because DB is empty",
        }

    context = {
        "sensor": sensor,
        "labels": labels,
        "values": values,
    }


    return render(request, 'main/dashboard.html',context)

# Get sensor data
def get_sensor_data(request):
    try:
        sensors = list(db["sensors"].find())
        data = {sensor["id"]: {
            "status": sensor["status"],
            "value": sensor["value"],
            "name": sensor["name"],  # Include name here
        } for sensor in sensors}
        return JsonResponse(data, safe=False)  # Explicitly return JSON
    except Exception as e:
        return JsonResponse({"error": str(e)}, status=500)


# Settings view
def settings_view(request):
    return render(request, 'main/settings.html')





#AUTH SETTINGS


# apps/accounts/views.py

from django.shortcuts import render, redirect
from django.contrib.auth import authenticate, login, logout, get_user_model
from django.contrib.auth.decorators import login_required
from django.contrib import messages

User = get_user_model()


@login_required
def manage_users_view(request):
    # Asigurăm accesul doar pentru utilizatorii master sau superuser
    if not (request.user.is_master or request.user.is_superuser):
        messages.error(request, 'Acces interzis. Nu aveți permisiunea necesară.')
        return redirect('main:profile')

    # Căutare: se poate căuta după username, email, prenume sau nume
    query = request.GET.get('q', '')
    if query:
        users = User.objects.filter(
            Q(username__icontains=query) |
            Q(email__icontains=query) |
            Q(first_name__icontains=query) |
            Q(last_name__icontains=query)
        )
    else:
        users = User.objects.all()

    if request.method == "POST":
        action = request.POST.get('action')
        user_id = request.POST.get('user_id')
        try:
            user_to_update = User.objects.get(pk=user_id)
        except User.DoesNotExist:
            messages.error(request, "Utilizatorul nu a fost găsit.")
            return redirect('main:manage_users')

        if action == "update_role":
            new_role = request.POST.get('role')
            if new_role in dict(User.ROLE_CHOICES):
                user_to_update.role = new_role
                user_to_update.save()
                messages.success(request, f"Rolul pentru utilizatorul {user_to_update.username} a fost actualizat.")
            else:
                messages.error(request, "Rol invalid selectat.")
        elif action == "delete_user":
            # Evităm ștergerea propriului cont, de exemplu
            if user_to_update == request.user:
                messages.error(request, "Nu îți poți șterge propriul cont.")
            else:
                username = user_to_update.username
                user_to_update.delete()
                messages.success(request, f"Utilizatorul {username} a fost șters cu succes.")
        return redirect('main:manage_users')

    context = {
        'users': users,
        'query': query,
        'ROLE_CHOICES': User.ROLE_CHOICES,
    }
    return render(request, 'main/manage_users.html', context)




def login_view(request):
    if request.method == 'POST':
        username = request.POST.get('username')
        password = request.POST.get('password')
        user = authenticate(request, username=username, password=password)
        if user:
            login(request, user)
            messages.success(request, 'Te-ai conectat cu succes.')
            return redirect('main:profile')
        else:
            messages.error(request, 'Username sau parolă incorecte.')
    return render(request, 'main/login.html')

def register_view(request):
    if request.method == 'POST':
        username  = request.POST.get('username')
        email     = request.POST.get('email')
        password1 = request.POST.get('password1')
        password2 = request.POST.get('password2')

        if password1 != password2:
            messages.error(request, 'Parolele nu se potrivesc.')
            return render(request, 'accounts/register.html')

        if User.objects.filter(username=username).exists():
            messages.error(request, 'Username-ul este deja folosit.')
            return render(request, 'main/register.html')

        # Creăm un nou utilizator
        user = User.objects.create_user(username=username, email=email, password=password1)
        messages.success(request, 'Contul a fost creat cu succes. Te rog conectează-te.')
        return redirect('main:login')

    return render(request, 'main/register.html')

@login_required
def logout_view(request):
    logout(request)
    messages.success(request, 'Te-ai deconectat cu succes.')
    return redirect('main:login')

@login_required
def profile_view(request):
    # Afișează profilul utilizatorului
    return render(request, 'main/profile.html', {'user': request.user})

@login_required
def edit_account_view(request):
    user = request.user
    if request.method == 'POST':
        username   = request.POST.get('username')
        email      = request.POST.get('email')
        first_name = request.POST.get('first_name')
        last_name  = request.POST.get('last_name')

        # Verificăm dacă username-ul este deja folosit de alt utilizator
        if User.objects.filter(username=username).exclude(pk=user.pk).exists():
            messages.error(request, 'Username-ul este deja folosit de alt utilizator.')
            return render(request, 'main/edit_account.html', {'user': user})

        user.username   = username
        user.email      = email
        user.first_name = first_name
        user.last_name  = last_name
        user.save()
        messages.success(request, 'Contul a fost actualizat cu succes.')
        return redirect('main:profile')

    return render(request, 'main/edit_account.html', {'user': user})

@login_required
def delete_account_view(request):
    if request.method == 'POST':
        request.user.delete()
        messages.success(request, 'Contul tău a fost șters.')
        return redirect('main:register')
    return render(request, 'main/delete_account.html')



