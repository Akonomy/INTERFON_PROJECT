from django.shortcuts import render, redirect
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse
from .forms import LoginForm, RegisterForm
from django.contrib import messages
from django.contrib.auth import get_user_model
from pymongo import MongoClient
import random

# MongoDB connection
client = MongoClient("mongodb://localhost:27017/")
db = client["pai"]

User = get_user_model()

# Custom error views
def custom_403_view(request, exception=None):
    return render(request, '403.html', status=403)

def custom_404_view(request, exception=None):
    return render(request, '404.html', status=404)

def custom_500_view(request):
    return render(request, '500.html', status=500)

# Login view
def login_view(request):
    error = None
    if request.method == 'POST':
        username = request.POST.get('username')
        password = request.POST.get('password')
        user = authenticate(request, username=username, password=password)
        if user is not None:
            login(request, user)
            return redirect('main:dashboard')  # Redirect to dashboard after login
        else:
            error = "Eroare la autentificare! Adaugați utilizatorul și parola primită!"
    
    return render(request, 'main/login.html', {'error': error})

# Register view
def register_view(request):
    if request.method == 'POST':
        form = RegisterForm(request.POST)
        if form.is_valid():
            username = form.cleaned_data['username']
            email = form.cleaned_data['email']
            password = form.cleaned_data['password']
            
            # Create the new user
            user = User.objects.create_user(username=username, email=email, password=password)
            user.save()
            
            # Automatically log in the new user after registration
            login(request, user)
            return redirect('main:dashboard')  # Redirect to dashboard after registration
        else:
            messages.error(request, 'A apărut o eroare la înregistrare!')
    else:
        form = RegisterForm()

    return render(request, 'main/register.html', {'form': form})

# Dashboard view
@login_required
def dashboard_view(request):
    sensors = list(db["sensors"].find())  # Fetch all sensors
    active_sensors = [sensor for sensor in sensors if sensor.get("active", False)]
    
    # Group sensors by type and mode
    analog_outputs = [sensor for sensor in active_sensors if sensor["type"] == "output" and sensor["mode"] == "analog"]
    digital_outputs = [sensor for sensor in active_sensors if sensor["type"] == "output" and sensor["mode"] == "digital"]
    analog_inputs = [sensor for sensor in active_sensors if sensor["type"] == "input" and sensor["mode"] == "analog"]
    digital_inputs = [sensor for sensor in active_sensors if sensor["type"] == "input" and sensor["mode"] == "digital"]

    return render(request, 'main/dashboard.html', {
        "analog_outputs": analog_outputs,
        "digital_outputs": digital_outputs,
        "analog_inputs": analog_inputs,
        "digital_inputs": digital_inputs,
    })

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

# Profile view
@login_required
def profile_view(request):
    # Determine user type based on is_superuser flag
    user_type = "Administrator" if request.user.is_superuser else "Utilizator"
    
    # Generate random phone number
    phone_number = f"+40 7{random.randint(20, 39)} {random.randint(100, 999)} {random.randint(100, 999)}"
    
    # Full list of Romanian county capitals (cities)
    cities = [
        "Alba Iulia", "Arad", "Bacău", "Baia Mare", "Bistrița", "Botoșani", "Brașov", "Brăila", "București",
        "Buzău", "Călărași", "Cluj-Napoca", "Constanța", "Craiova", "Deva", "Drobeta-Turnu Severin", "Focșani",
        "Galați", "Giurgiu", "Iași", "Pitești", "Ploiești", "Oradea", "Reșița", "Satu Mare", "Sfântu Gheorghe",
        "Sibiu", "Slatina", "Slobozia", "Suceava", "Târgu Jiu", "Târgu Mureș", "Timișoara", "Tulcea", "Vaslui",
        "Vatra Dornei", "Zalău", "Râmnicu Vâlcea", "Bârlad", "Făgăraș"
    ]
    
    # Common Romanian street names
    street_names = [
        "Mihai Viteazul", "Unirii", "Eroilor", "Primăverii", "Libertății", "Victoriei", "Independenței", 
        "Stefan cel Mare", "Calea București", "Calea Clujului", "Florilor", "Păcii", "Traian", "Carpaților",
        "George Coșbuc", "Griviței", "Horea", "Iuliu Maniu", "Alexandru Ioan Cuza", "Petru Rareș", 
        "Barbu Ștefănescu Delavrancea", "Ion Creangă", "Nicolae Bălcescu", "Vasile Alecsandri", "Eminescu"
    ]
    
    # Generate random address
    street_number = random.randint(1, 500)  # Larger range of street numbers
    random_street = random.choice(street_names)
    random_city = random.choice(cities)
    address = f"Str. {random_street} {street_number}, {random_city}, România"
    
    # Mock subscription level
    subscriptions = ["Basic", "Standard", "Premium", "Pro"]
    subscription = random.choice(subscriptions)
    
    return render(request, 'main/profile.html', {
        "user_type": user_type,
        "additional_data": {
            "phone": phone_number,
            "address": address,
            "subscription": subscription
        }
    })

# Logout view
def logout_view(request):
    logout(request)
    return redirect('main:login')

# Settings view
def settings_view(request):
    return render(request, 'main/settings.html')
