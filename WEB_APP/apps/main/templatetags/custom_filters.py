from django import template

register = template.Library()

@register.filter
def value_to_kelvin(value):
    """
    Converts a raw sensor value to Kelvin.
    - 0 maps to 'OFF'
    - Values <= 50 map to '2700K'
    - Values <= 128 map to '4500K'
    - Values <= 255 map to '6000K'
    """
    if value == 0:
        return "OFF"
    elif value <= 50:
        return "2700K"
    elif value <= 128:
        return "4500K"
    elif value <= 255:
        return "6000K"
    return f"{value}K"

@register.filter
def value_to_celsius(value):
    """
    Maps raw sensor values (0-255) to a Celsius range (10-40°C).
    - 0 maps to 'OFF'
    - Value is scaled linearly between 10°C and 40°C.
    """
    if value == 0:
        return "OFF"
    celsius = ((value / 255) * (40 - 10)) + 10
    return f"{round(celsius)}°C"

@register.filter
def uppercase(value):
    return str(value).upper()