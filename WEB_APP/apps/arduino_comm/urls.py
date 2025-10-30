from django.urls import path
from . import views



from .views import internal_enqueue_command






app_name = 'arduino_comm'

urlpatterns = [


    path("sensor_list/", views.sensor_list, name="sensor_list"),
    path("sensors/<int:sensor_id>/edit/", views.sensor_edit, name="sensor_edit"),
    path("sensors/<int:sensor_id>/delete/", views.sensor_delete, name="sensor_delete"),
    path('sensor_add/', views.sensor_add, name='sensor_add'),

    path("access-logs/", views.access_log_list, name="access_log_list"),
    path('sendcommand/', views.web_send_command, name='web_send_command'),


    path("api/sensor/update/", views.api_update_sensor, name="api_update_sensor"),
    path("api/accesslog/", views.api_log_access, name="api_log_access"),


    path('api/auth/challenge/', views.request_challenge, name='request_challenge'),
    path('api/auth/respond/', views.respond_challenge, name='respond_challenge'),
    path('api/tag/check/', views.api_tag_check, name='api_tag_check'),

    path('api/commands/poll/', views.public_poll_commands, name='public_poll_commands'),
    path('api/commands/ack/', views.public_acknowledge_command, name='public_acknowledge_command'),

    path('api/internal/commands/enqueue/', views.internal_enqueue_command, name='internal_enqueue_command'),



    path('syslog/', views.syslog_view, name='syslog_view'),

    path("api/syslog/", views.syslog_api_receiver, name="syslog_api_receiver"),
]


