// Command Functions
function sendPresetValue(sensorId, value) {
       //document.getElementById(`value-display-${sensorId}`).innerText = value; // Update display with preset value
       // document.querySelector(`#sensor-${sensorId} .range-slider`).value = value; 
        updateValueDisplay(sensorId, value);


        // Update slider to reflect preset value


        



fetch(sendCommandUrl, {
        method: "POST",
        headers: { 
            "Content-Type": "application/json", 
            "X-CSRFToken": csrfToken 
        },
        body: JSON.stringify({ sensor_id: sensorId, command: "set_value", value: value })
    })
    .then(response => response.json())
    .then(data => console.log(data.message || "Value updated successfully"));
}



function sendDigitalCommand(sensorId, status) {
    fetch(sendCommandUrl, {
        method: "POST",
        headers: { 
            "Content-Type": "application/json", 
            "X-CSRFToken": csrfToken 
        },
        body: JSON.stringify({ sensor_id: sensorId, command: status })
    })
    .then(response => response.json())
    .then(data => console.log(data.message || "Command sent successfully"));
}

function sendAnalogValueDebounced(sensorId, value) {
    clearTimeout(debounceTimeout);
    debounceTimeout = setTimeout(() => {
        sendAnalogValue(sensorId, value); // Call the server update function after debounce delay
    }, 200); // Debounce delay
}


function sendAnalogValue(sensorId, value) {
    //document.getElementById(`value-display-${sensorId}`).innerText = value;
    updateValueDisplay(sensorId, value);
    fetch(sendCommandUrl, {
        method: "POST",
        headers: { 
            "Content-Type": "application/json", 
            "X-CSRFToken": csrfToken 
        },
        body: JSON.stringify({ sensor_id: sensorId, command: "set_value", value: value })
    })
    .then(response => response.json())
    .then(data => console.log(data.message || "Value updated successfully"));
}

let previousSensorData = {};

// Data Fetching and Update
function fetchSensorData() {
    fetch(sensorDataUrl)
        .then(response => response.json())
        .then(data => {
            updateDivsIfDataChanged(data);
            previousSensorData = data;
        })
        .catch(error => console.error('Error fetching sensor data:', error));
}

function updateDivsIfDataChanged(newData) {
    let isChanged = false;

    for (let sensorId in newData) {
        const sensorData = newData[sensorId];
        const previousData = previousSensorData[sensorId];

        if (!previousData || JSON.stringify(sensorData) !== JSON.stringify(previousData)) {
            isChanged = true;
        }
    }

    if (isChanged) {
        for (let sensorId in newData) {
            updateSensorDiv(sensorId, newData[sensorId]);
        }
    }
}

// Sensor Update Functions
function updateSensorDiv(sensorId, sensorData) {
    const sensorDiv = document.getElementById(`sensor-${sensorId}`);
    if (sensorDiv) {
        const previousData = previousSensorData[sensorId] || {};

        // Update text and styles
        updateSensorText(sensorDiv, sensorData);
        updateSensorStyles(sensorDiv, sensorId, sensorData);
        updateValueDisplay(sensorId, sensorData.value); // Use sensorData.value instead of undefined value

        // Add notifications based on changes
if (previousData.status !== sensorData.status) {
    const action = sensorData.status === "high" ? "LOCK" : "UNLOCK";
    let customMessage;

    // Custom messages for specific sensors
    switch (sensorId) {
        case "sensor_1":
            customMessage = `OFFICE DOOR IS  ${action}ING`;
             addNotification("warning", ``, customMessage);
            break;
        case "sensor_2":
            customMessage = `MAIN DOOR IS ${action}ING`;
             addNotification("warning", ``, customMessage);
            break;
        case "sensor_5":
            customMessage = `Custom message for Sensor 5: ${action}`;
            break;
        case "sensor_6":
            customMessage = `Custom message for Sensor 6: ${action}`;
            break;
        case "sensor_8":
            customMessage = `OFFICE  IS ${action}ED`;
             addNotification("warning", ``, customMessage);
            break;
        case "sensor_9":
            customMessage = `MAIN  IS ${action}ED`;
             addNotification("warning", ``, customMessage);
            break;
        default:
            customMessage = ``;
    }

   
}

if (previousData.value !== sensorData.value) {
    let customValueMessage;

    // Custom value messages for specific sensors
    switch (sensorId) {
        case "out_sensor_1":
            customValueMessage = `  ${sensorData.value}.`;
            break;
        case "out_sensor_2":
            customValueMessage = ` ${sensorData.value}.`;
            break;
        case "sensor_3":
            customValueMessage = `LIGHT CHANGED `;
            if(sensorData.value==0){
                customValueMessage = `LIGHT OFF `;
            }
            addNotification("info", ``, customValueMessage);
            break;
        case "sensor_4":
            customValueMessage = `TEMPERATURE CHANGED `;
            if(sensorData.value==0){
                customValueMessage = `AIR CONDITIONER OFF `;
            }
            addNotification("info", ``, customValueMessage);
            break;
        case "out_sensor_8":
            customValueMessage = `Sensor 8 value adjusted to ${sensorData.value}.`;
            break;
        case "out_sensor_9":
            customValueMessage = `Sensor 9 updated with value ${sensorData.value}.`;
            break;
        default:
            customValueMessage = ``;
    }

    
}
    }
}

// Separate Text Updates
function updateSensorText(sensorDiv, sensorData) {
    const statusElement = sensorDiv.querySelector(".sensor-status");
    if (statusElement) {
        statusElement.textContent = sensorData.status;
    }

    const valueElement = sensorDiv.querySelector(".sensor-value");
    if (valueElement) {
        valueElement.textContent = sensorData.value;
    }
}
function updateSensorStyles(sensorDiv, sensorId, sensorData) {
    sensorDiv.classList.remove("bg-blue", "bg-orange");

    // Set background color based on sensor ID and status
    if (sensorData.status === "high" && sensorId === "sensor_1") {
        sensorDiv.classList.add("bg-blue");
    }
    if (sensorData.status === "high" && sensorId === "sensor_2") {
        sensorDiv.classList.add("bg-orange");
    }

    if (sensorData.value === 1) {
        if (sensorId === "sensor_8") {
            // Control lock icon for sensor 8
            const lockIcon = document.querySelector("#sensor-sensor_1 .lock-icon");
            if (lockIcon) {
                lockIcon.classList.add("lock-blue");
                lockIcon.classList.remove("lock-invisible");
            }
        } else if (sensorId === "sensor_9") {
            // Control lock icon for sensor 9
            const lockIcon = document.querySelector("#sensor-sensor_2 .lock-icon");
            if (lockIcon) {
                lockIcon.classList.add("lock-orange");
                lockIcon.classList.remove("lock-invisible");
            }
        }
    } else {
        // Make lock icons invisible if value is not 1
        if (sensorId === "sensor_8") {
            const lockIcon = document.querySelector("#sensor-sensor_1 .lock-icon");
            if (lockIcon) {
                lockIcon.classList.add("lock-invisible");
                lockIcon.classList.remove("lock-blue");
            }
        } else if (sensorId === "sensor_9") {
            const lockIcon = document.querySelector("#sensor-sensor_2 .lock-icon");
            if (lockIcon) {
                lockIcon.classList.add("lock-invisible");
                lockIcon.classList.remove("lock-orange");
            }
        }
    }

    const alpha = sensorData.value / 255;
    sensorDiv.style.backgroundColor = "";

   
    // Interpolate colors for sensor_3 (warm orange to cold blue)
    if (sensorId === "sensor_3") {
        const startColor = { r: 199, g: 125, b: 73 }; // Warm orange
        const endColor = { r: 15, g: 138, b: 100 }; // Cold blue

        const blendedColor = {
            r: Math.round(startColor.r * (1 - alpha) + endColor.r * alpha),
            g: Math.round(startColor.g * (1 - alpha) + endColor.g * alpha),
            b: Math.round(startColor.b * (1 - alpha) + endColor.b * alpha),
        };

        sensorDiv.style.backgroundColor = `rgba(${blendedColor.r}, ${blendedColor.g}, ${blendedColor.b}, ${alpha})`;
    }

    // Interpolate colors for sensor_4 (cold blue to hot red)
    else if (sensorId === "sensor_4") {
        const startColor = { r: 15, g: 89, b: 120 }; // Cold blue
        const endColor = { r: 38, g: 0, b: 0 }; // Hot red

        const blendedColor = {
            r: Math.round(startColor.r * (1 - alpha) + endColor.r * alpha),
            g: Math.round(startColor.g * (1 - alpha) + endColor.g * alpha),
            b: Math.round(startColor.b * (1 - alpha) + endColor.b * alpha),
        };

        sensorDiv.style.backgroundColor = `rgba(${blendedColor.r}, ${blendedColor.g}, ${blendedColor.b}, ${alpha})`;
    }
}

// Notification Functions
function addNotification(type, sensorName, message) {
    const notificationsList = document.querySelector(".notifications-list");
    if (!notificationsList) return;

    // Check the current number of notifications
    const existingNotifications = notificationsList.querySelectorAll(".notification-item");
    if (existingNotifications.length >= 5) {
        // Remove the oldest notification
        notificationsList.removeChild(existingNotifications[0]);
    }

    // Create a new notification item
    const notificationItem = document.createElement("li");
    notificationItem.classList.add("notification-item");

    // Determine icon based on type
    let iconClass;
    switch (type) {
        case "info":
            iconClass = "fa-info-circle";
            break;
        case "success":
            iconClass = "fa-check-circle";
            break;
        case "warning":
            iconClass = "fa-exclamation-circle";
            break;
        case "danger":
            iconClass = "fa-times-circle";
            break;
        default:
            iconClass = "fa-info-circle";
    }

    // Build notification content
    notificationItem.innerHTML = `
        <i class="fa ${iconClass} notification-icon"></i>
        <strong>${sensorName}:</strong> ${message}
    `;

    // Append the notification to the list
    notificationsList.appendChild(notificationItem);

    // Automatically remove the notification after a delay (e.g., 10 seconds)
    setTimeout(() => notificationItem.remove(), 5500);
}



// Map sensor values to Kelvin or Celsius
function mapToKelvin(value) {
    if (value === 0) return "OFF";
    if (value <= 50) return "2700K";
    if (value <= 128) return "4500K";
    if (value <= 255) return "6000K";
}

function mapToCelsius(value) {
    if (value === 0) return "OFF";
    const celsius = ((value / 255) * (40 - 10)) + 10;
    return `${Math.round(celsius)}°C`;
}

// Toggle Sensor Activation
function toggleSensor(sensorId, isActive) {
    const status = isActive ? "activate" : "deactivate";
    sendDigitalCommand(sensorId, status);
}


// Update the value display based on sensor type
function updateValueDisplay(sensorId, value) {
    const displayElement = document.getElementById(`value-display-${sensorId}`);
    if (!displayElement) return;

    if (sensorId === "sensor_3") {
        displayElement.innerText = mapToKelvin(value);
    } else if (sensorId === "sensor_4") {
        displayElement.innerText = mapToCelsius(value);
    }
}



// Automatic Data Fetching
setInterval(fetchSensorData, 1800);
