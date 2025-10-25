from django.urls import path
from . import views

app_name = 'arduino_comm'

urlpatterns = [
    path('leds/', views.led_control_page, name='led_control_page'),
    path('leds/toggle/<int:led_id>/', views.toggle_led, name='toggle_led'),
    path('leds/register/', views.register_led, name='register_led'),



    path('api/auth/challenge/', views.request_challenge, name='request_challenge'),
    path('api/auth/respond/', views.respond_challenge, name='respond_challenge'),
    path('api/tag/check/', views.api_tag_check, name='api_tag_check'),
]


