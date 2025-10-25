"""PAI_APP URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/3.2/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  path('', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  path('', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.urls import include, path
    2. Add a URL to urlpatterns:  path('blog/', include('blog.urls'))
"""
from django.contrib import admin
from django.urls import path, include
from apps.main import views as main_views  # Import the main app's views

from django.conf.urls import handler403, handler404, handler500
from apps.main.views import custom_403_view, custom_404_view, custom_500_view

handler403 = 'apps.main.views.custom_403_view'
handler404 = 'apps.main.views.custom_404_view'
handler500 = 'apps.main.views.custom_500_view'


urlpatterns = [
    path('admin/', admin.site.urls),
    path('', include('apps.main.urls')),

     path('', main_views.dashboard_view, name='root_dashboard'),

    path('', include('apps.arduino_comm.urls', namespace='arduino_comm')),  # New app
]



