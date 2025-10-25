#include <ArduinoJson.h>  // Include ArduinoJson library

// Pin Definitions
const int ledPins[] = {2, 3, 5, 6};  // LEDs for command control
const int inputPin12 = 12;           // Input pin for sensor_8
const int inputPin13 = 13;           // Input pin for sensor_9

int lastState12 = LOW;               // Last state of pin 12
int lastState13 = LOW;               // Last state of pin 13

void setup() {
  // Initialize LED pins as output
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);  // Ensure LEDs are off at the start
  }
  
  // Initialize input pins
  pinMode(inputPin12, INPUT);
  pinMode(inputPin13, INPUT);

  // Start serial communication
  Serial.begin(9600);
}

void loop() {
  // Check for incoming commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');  // Read the incoming command until newline
    if (command == "ping") {  // Handshake signal from Python
      Serial.println("pong");
    } else {
      handleCommand(command);  // Process other commands
    }
  }

  // Monitor sensor state changes and send updates
  checkAndReportSensorState(inputPin12, lastState12, "sensor_8");
  checkAndReportSensorState(inputPin13, lastState13, "sensor_9");

  delay(100);  // Small delay to avoid spamming the serial buffer
}

void handleCommand(const String &command) {
  if (command.startsWith("sensor_")) {
    // Parse the command dynamically
    int underscoreIndex1 = command.indexOf('_');
    int underscoreIndex2 = command.indexOf('_', underscoreIndex1 + 1);

    String sensorId = command.substring(underscoreIndex1 + 1, underscoreIndex2);
    String action = command.substring(underscoreIndex2 + 1);

    int sensorIndex = sensorId.toInt();  // Convert sensor ID to an index

    if (sensorIndex >= 1 && sensorIndex <= 4) {
      int ledPin = ledPins[sensorIndex - 1];
      
      if (action == "high") {
        digitalWrite(ledPin, HIGH);
      } else if (action == "low") {
        digitalWrite(ledPin, LOW);
      } else if (action.startsWith("value_")) {
        // For analog control (PWM)
        int pwmValue = action.substring(6).toInt();
        analogWrite(ledPin, pwmValue);
      }
    }
  }
}

void checkAndReportSensorState(int pin, int &lastState, const char *sensorName) {
  int currentState = digitalRead(pin);

  if (currentState != lastState) {
    lastState = currentState;

    // Create a JSON object to report the state change
    StaticJsonDocument<64> doc;
    doc[sensorName] = (currentState == HIGH) ? "high" : "low";

    // Serialize the JSON object and send it over Serial
    String output;
    serializeJson(doc, output);
    Serial.println(output);
  }
}
