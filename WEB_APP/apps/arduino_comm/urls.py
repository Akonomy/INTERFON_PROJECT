from django.urls import path
from . import views



from .views import internal_enqueue_command






app_name = 'arduino_comm'

urlpatterns = [
    path('leds/', views.led_control_page, name='led_control_page'),
    path('leds/toggle/<int:led_id>/', views.toggle_led, name='toggle_led'),
    path('leds/register/', views.register_led, name='register_led'),



    path('api/auth/challenge/', views.request_challenge, name='request_challenge'),
    path('api/auth/respond/', views.respond_challenge, name='respond_challenge'),
    path('api/tag/check/', views.api_tag_check, name='api_tag_check'),

    path('api/commands/poll/', views.public_poll_commands, name='public_poll_commands'),
    path('api/commands/ack/', views.public_acknowledge_command, name='public_acknowledge_command'),

    path('api/internal/commands/enqueue/', views.internal_enqueue_command, name='internal_enqueue_command'),

     path('sendcommand/', views.web_send_command, name='web_send_command'),
]


