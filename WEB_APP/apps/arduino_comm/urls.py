from django.urls import path
from . import views



from .views import internal_enqueue_command

from . import views_tags as tag_views




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
    path('api/tag/info/' , views.api_tag_get_info, name= 'api_get_info'),
    path('api/commands/poll/', views.public_poll_commands, name='public_poll_commands'),
    path('api/commands/ack/', views.public_acknowledge_command, name='public_acknowledge_command'),

    path('api/internal/commands/enqueue/', views.internal_enqueue_command, name='internal_enqueue_command'),



    path('syslog/', views.syslog_view, name='syslog_view'),

    path("api/syslog/", views.syslog_api_receiver, name="syslog_api_receiver"),



     # ──────── Personal Management ────────
    path("people/", views.personal_list, name="personal_list"),
    path("people/add/", views.personal_add, name="personal_add"),
    path("people/<int:person_id>/edit/", views.personal_edit, name="personal_edit"),
    path("people/<int:person_id>/delete/", views.personal_delete, name="personal_delete"),

 # ──────── Tag Management (New View Module) ────────
    path("tags/", tag_views.tag_list, name="tag_list"),
    path("tags/add/", tag_views.tag_add, name="tag_add"),
    path("tags/<int:tag_id>/edit/", tag_views.tag_edit, name="tag_edit"),
    path("tags/<int:tag_id>/delete/", tag_views.tag_delete, name="tag_delete"),
    path("tags/<int:tag_id>/allow/", tag_views.tag_allow, name="tag_allow"),
    path("tags/<int:tag_id>/revoke/", tag_views.tag_revoke, name="tag_revoke"),

    path("tags/register-requests/", tag_views.tag_register_requests, name="tag_register_requests"),
    path("tags/revoke-requests/", tag_views.tag_revoke_requests, name="tag_revoke_requests"),

    path("api/tag/register-request/", tag_views.api_register_tag_request, name="api_register_tag_request"),
    path("api/tag/revoke-request/", tag_views.api_revoke_tag_request, name="api_revoke_tag_request"),

    path("tags/register-requests/<int:request_id>/approve/", tag_views.approve_tag_register_request, name="approve_tag_register_request"),
    path("tags/register-requests/<int:request_id>/reject/",tag_views.reject_tag_register_request, name="reject_tag_register_request"),

    # Tag revoke approval/rejection
    path("tags/revoke-requests/<int:request_id>/approve/", tag_views.approve_tag_revoke_request, name="approve_tag_revoke_request"),
    path("tags/revoke-requests/<int:request_id>/reject/", tag_views.reject_tag_revoke_request, name="reject_tag_revoke_request"),

]



