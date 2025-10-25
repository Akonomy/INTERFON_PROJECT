import serial
import serial.tools.list_ports
import requests
import time
import json
import threading
import tkinter as tk
from queue import Queue
from ArduinoSiimport serial
import serial.tools.list_ports
import requests
import time
import json

# Django server configuration
DJANGO_FETCH_URL = "http://127.0.0.1:8000/fetch/"  # Django API endpoint to fetch data
DJANGO_UPDATE_URL = "http://127.0.0.1:8000/receive/"  # Django API endpoint to receive sensor updates

def find_arduino_port():
    """Search for the Arduino's COM port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        try:
            # Attempt to connect to the port
            with serial.Serial(port.device, 9600, timeout=2) as ser:
                print(f"Found potential port: {port.device}")
                time.sleep(2)  # Allow time for Arduino to reset
                if ser.is_open:
                    ser.write(b'ping\n')  # Optionally send a handshake signal
                    response = ser.readline().decode('utf-8').strip()
                    if response:  # If the Arduino responds
                        print(f"Arduino found at {port.device}")
                        return port.device
        except (serial.SerialException, OSError) as e:
            pass  # Ignore and continue
    print("No Arduino detected on available COM ports.")
    return None

# Find the Arduino port
arduino_port = find_arduino_port()
arduino_serial = None

if arduino_port:
    try:
        arduino_serial = serial.Serial(arduino_port, 9600, timeout=2)
        if arduino_serial.is_open:
            print(f"Serial port {arduino_port} opened successfully.")
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {arduino_port}.", e)

def fetch_data_from_django():
    """Fetch latest sensor data from Django API and display it."""
    try:
        response = requests.get(DJANGO_FETCH_URL)
        if response.status_code == 200:
            sensor_data = response.json()
            print("Current sensor data from Django:")
            for sensor_id, data in sensor_data.items():
                if data["active"]:  # Only consider active sensors
                    print(f"  {sensor_id}: {data}")
            return {k: v for k, v in sensor_data.items() if v["active"]}  # Return active sensors only
        else:
            print("Failed to fetch data:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print("Error fetching data from Django:", e)
    return {}

def send_command_to_arduino(command):
    """Send command to Arduino directly."""
    if arduino_serial:
        try:
            arduino_serial.write((command + "\n").encode('utf-8'))
            print("Sent command to Arduino:", command)
        except Exception as e:
            print("Error sending command to Arduino:", e)

def send_data_to_django(sensor_data):
    """Send sensor data to Django API."""
    value_mapping = {'high': 1, 'low': 0}
    processed_data = {}
    for key, value in sensor_data.items():
        if value in value_mapping:
            processed_data[key] = value_mapping[value]
        else:
            processed_data[key] = value  # Assuming it's already an integer
    try:
        response = requests.post(DJANGO_UPDATE_URL, json=processed_data)
        if response.status_code == 200:
            print("Data sent successfully:", sensor_data)
        else:
            print("Failed to send data:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print("Error sending data to Django:", e)

def read_from_arduino():
    """Read data from Arduino and process sensor_8 and sensor_9 states."""
    if arduino_serial and arduino_serial.in_waiting > 0:
        try:
            data = arduino_serial.readline().decode('utf-8').strip()
            print("Received data from Arduino:", data)
            # Parse JSON data
            try:
                json_data = json.loads(data)
                if "sensor_8" in json_data or "sensor_9" in json_data:
                    send_data_to_django(json_data)  # Send data to Django only if relevant
            except json.JSONDecodeError:
                print("Invalid JSON format received from Arduino.")
        except Exception as e:
            print("Error reading from Arduino:", e)

def main():
    print("Starting Arduino to Web communication...")
    while True:
        # Read from Arduino for input states
        read_from_arduino()
        
        # Fetch and handle active sensors from Django
        django_data = fetch_data_from_django()

        if django_data:
            for sensor_id, data in django_data.items():
                if sensor_id == "sensor_1" or sensor_id == "sensor_2":  # Digital-only sensors
                    if data["mode"] == "digital":
                        command = f"{sensor_id}_{data['status']}"
                    else:
                        command = f"{sensor_id}_{'high' if data['value'] > 127 else 'low'}"
                    send_command_to_arduino(command)

                elif sensor_id == "sensor_3" or sensor_id == "sensor_4":  # Supports both digital and analog
                    if data["mode"] == "analog":
                        command = f"{sensor_id}_value_{data['value']}"
                    else:
                        command = f"{sensor_id}_{data['status']}"
                    send_command_to_arduino(command)

        time.sleep(1)

if __name__ == "__main__":
    main()mulator import ArduinoSimulator  # Import the simulator

# Django server configuration
DJANGO_FETCH_URL = "http://127.0.0.1:8000/fetch/"  # Django API endpoint to fetch data
DJANGO_UPDATE_URL = "http://127.0.0.1:8000/receive/"  # Django API endpoint to receive sensor updates

# Global variables
arduino_serial = None
simulator = None
data_queue = Queue()  # Queue to exchange data between threads


def find_arduino_port():
    """Search for the Arduino's COM port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        try:
            with serial.Serial(port.device, 9600, timeout=2) as ser:
                print(f"Found potential port: {port.device}")
                time.sleep(2)
                if ser.is_open:
                    ser.write(b'ping\n')
                    response = ser.readline().decode('utf-8').strip()
                    if response:
                        print(f"Arduino found at {port.device}")
                        return port.device
        except (serial.SerialException, OSError):
            pass
    print("No Arduino detected on available COM ports.")
    return None


def initialize_simulator():
    """Initialize the Arduino simulator."""
    global simulator
    if simulator is None:
        simulator = ArduinoSimulator()  # Initialize the simulator
    print("Simulator initialized.")
    return simulator


def fetch_data_from_django():
    """Fetch latest sensor data from Django API."""
    try:
        response = requests.get(DJANGO_FETCH_URL)
        if response.status_code == 200:
            sensor_data = response.json()
            print("Current sensor data from Django:")
            for sensor_id, data in sensor_data.items():
                if data["active"]:
                    print(f"  {sensor_id}: {data}")
            return {k: v for k, v in sensor_data.items() if v["active"]}
        else:
            print("Failed to fetch data:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print("Error fetching data from Django:", e)
    return {}


def send_data_to_django(sensor_data):
    """Send sensor data to Django API."""
    try:
        response = requests.post(DJANGO_UPDATE_URL, json=sensor_data)
        if response.status_code == 200:
            print("Data sent successfully:", sensor_data)
        else:
            print("Failed to send data:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print("Error sending data to Django:", e)



def process_simulator():
    """Process data from simulator and sync with Django."""
    while True:

        # Fetch data from Django and update the simulator
        django_data = fetch_data_from_django()
        if django_data:
            for sensor_id, data in django_data.items():
                # Update simulator's GUI state based on Django data
                if sensor_id in ["sensor_1", "sensor_2"]:
                    # Handle digital states
                    value = 255 if data["status"] == "high" else 0
                    simulator.send_data({sensor_id: value})
                elif sensor_id in ["sensor_3", "sensor_4"]:
                    # Handle PWM-controlled LEDs
                    if data["mode"] == "analog":
                        value = data["value"]
                        simulator.update_pwm(f"led_{sensor_id[-1]}", value)
                    else:
                        value = 255 if data["status"] == "high" else 0
                        simulator.send_data({sensor_id: value})

        # Get data from simulator and send to Django
        sensor_data = simulator.get_data()
        send_data_to_django(sensor_data)
                

        # Fetch data from Django and update the simulator
        django_data = fetch_data_from_django()
        if django_data:
            for sensor_id, data in django_data.items():
                # Update simulator's GUI state based on Django data
                if sensor_id in ["sensor_1", "sensor_2"]:
                    # Handle digital states
                    value = 255 if data["status"] == "high" else 0
                    simulator.send_data({sensor_id: value})
                elif sensor_id in ["sensor_3", "sensor_4"]:
                    # Handle PWM-controlled LEDs
                    if data["mode"] == "analog":
                        value = data["value"]
                        simulator.update_pwm(f"led_{sensor_id[-1]}", int(value))
                    else:
                        value = 255 if data["status"] == "high" else 0
                        simulator.send_data({sensor_id: value})

        time.sleep(3)  # Adjust sync frequency if needed



def main():
    """Main entry point."""
    global arduino_serial

    # Check for Arduino or fallback to simulator
    arduino_port = find_arduino_port()
    if arduino_port:
        try:
            arduino_serial = serial.Serial(arduino_port, 9600, timeout=2)
            if arduino_serial.is_open:
                print(f"Serial port {arduino_port} opened successfully.")
        except serial.SerialException as e:
            print(f"Error: Could not open serial port {arduino_port}.", e)
    else:
        initialize_simulator()

    # Start processing in a background thread
    threading.Thread(target=process_simulator, daemon=True).start()


if __name__ == "__main__":
    simulator = ArduinoSimulator()  # Initialize the simulator
    simulator.start()  # Start the simulator GUI
    main()  # Run the main logic in the background
    simulator.root.mainloop()  # Run the GUI event loop on the main thread
