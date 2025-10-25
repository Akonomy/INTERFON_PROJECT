import tkinter as tk
from tkinter import ttk
import json
import threading


class ArduinoSimulator:
    def __init__(self):
        # Start the tkinter root in a separate thread
        self.root = tk.Tk()
        self.root.title("Arduino Simulator")
        self.led_states = {"led_1": "off", "led_2": "off", "led_3": 0, "led_4": 0}
        self.confirm_states = {"X": True, "Y": True}  # Buttons controlling LED 1 and LED 2
        
        # Create LED Buttons
        self.led_buttons = {}
        for i in range(4):
            led_id = f"led_{i+1}"
            self.led_states[led_id] = "off"
            button = tk.Button(
                self.root,
                text=led_id,
                width=10,
                height=3,
                bg="grey",
                command=lambda id=led_id: self.toggle_led(id)
            )
            button.grid(row=0, column=i, padx=5, pady=5)
            self.led_buttons[led_id] = button
        
        # Confirmation Buttons
        self.confirm_buttons = {}
        for idx, confirm_id in enumerate(["X", "Y"]):
            button = tk.Button(
                self.root,
                text=f"Confirm Door",
                width=12,
                height=2,
                bg="green",
                command=lambda id=confirm_id: self.toggle_confirm(id)
            )
            button.grid(row=1, column=idx, padx=5, pady=5)
            self.confirm_buttons[confirm_id] = button
        
        # PWM Sliders for LED 3 and LED 4
       
        
        # State Label
        self.state_label = ttk.Label(self.root, text="Simulator casa automatizata")
        self.state_label.grid(row=3, column=0, columnspan=4)

    def start(self):
        """Start the tkinter mainloop in a separate thread."""
        self.thread = threading.Thread(target=self.root.mainloop, daemon=True)
        self.thread.start()

    def toggle_confirm(self, confirm_id):
        """Toggle confirmation button state."""
        self.confirm_states[confirm_id] = not self.confirm_states[confirm_id]
        color = "green" if self.confirm_states[confirm_id] else "red"
        self.confirm_buttons[confirm_id].configure(bg=color)
        self.update_led_states()

    def toggle_led(self, led_id):
        """Toggle LED on/off."""
        if led_id in ["led_1", "led_2"]:
            current_state = self.led_states[led_id]
            self.led_states[led_id] = "on" if current_state == "off" else "off"
            self.update_led_states()

    def update_pwm(self, led_id, value):
        """Update PWM value for LED 3 or LED 4."""
        pwm_value = int(value)
        self.led_states[led_id] = pwm_value
        color = self.pwm_to_color(pwm_value)
        self.led_buttons[led_id].configure(bg=color)

    def update_led_states(self):
        """Update LED 1 and LED 2 based on confirm button states."""
        if self.led_states["led_1"] == "on" and self.confirm_states["X"]:
            self.led_buttons["led_1"].configure(bg="yellow")
        else:
            self.led_buttons["led_1"].configure(bg="grey")
        
        if self.led_states["led_2"] == "on" and self.confirm_states["Y"]:
            self.led_buttons["led_2"].configure(bg="yellow")
        else:
            self.led_buttons["led_2"].configure(bg="grey")

    def pwm_to_color(self, pwm_value):
        """Convert PWM value to a color gradient."""
        return f"#ffff{255 - pwm_value:02x}"

    def get_data(self):
        """Return the current sensor data."""
        return {
            "sensor_8": 1 if self.led_states["led_1"] == "on" and self.confirm_states["X"] else 0,
            "sensor_9": 1 if self.led_states["led_2"] == "on" and self.confirm_states["Y"] else 0,
            "sensor_3": 0 if self.led_states["led_3"] == "off" else int(self.led_states["led_3"]),
            "sensor_4": 0 if self.led_states["led_4"] == "off" else int(self.led_states["led_4"])
        }

    def send_data(self, data):
        """Update simulator states based on external data."""
        if "sensor_1" in data:
            self.led_states["led_1"] = "on" if data["sensor_1"] else "off"
        if "sensor_2" in data:
            self.led_states["led_2"] = "on" if data["sensor_2"] else "off"
        if "sensor_3" in data:
            self.led_states["led_3"] = data["sensor_3"]
            self.update_pwm("led_3", data["sensor_3"])
        if "sensor_4" in data:
            self.led_states["led_4"] = data["sensor_4"]
            self.update_pwm("led_4", data["sensor_4"])
        self.update_led_states()
