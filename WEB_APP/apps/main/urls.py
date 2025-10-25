from django.urls import path
from . import views
from django.conf.urls import handler403, handler404, handler500


app_name = 'main'



handler403 = 'apps.main.views.custom_403_view'
handler404 = 'apps.main.views.custom_404_view'
handler500 = 'apps.main.views.custom_500_view'

urlpatterns = [
    path('login/', views.login_view, name='login'),
    path('register/', views.register_view, name='register'),
    path('dashboard/', views.dashboard_view, name='dashboard'),
    path('profile/', views.profile_view, name='profile'),
    path('logout/', views.logout_view, name='logout'),
    path('settings/', views.settings_view, name='settings'),  
    
    path('sensor-data/', views.get_sensor_data, name='sensor_data'),
]

