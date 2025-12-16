from django.urls import path
from . import views
from django.conf.urls import handler403, handler404, handler500

from .views import (
    login_view, register_view, logout_view, profile_view,
    edit_account_view, delete_account_view, manage_users_view
)


app_name = 'main'



handler403 = 'apps.main.views.custom_403_view'
handler404 = 'apps.main.views.custom_404_view'
handler500 = 'apps.main.views.custom_500_view'



urlpatterns = [
    path('login/', login_view, name='login'),
    path('register/', register_view, name='register'),
    path('logout/', logout_view, name='logout'),
    path('delete/', delete_account_view, name='delete_account'),
    path('edit/', edit_account_view, name='edit_account'),
    path('manage-users/', manage_users_view, name='manage_users'),
    path('profile', profile_view, name='profile'),
    path('settings/', views.settings_view, name='settings'),
    path('dashboard/', views.dashboard_view, name='dashboard'),
    path('', views.dashboard_view, name='dashboard'),
]

