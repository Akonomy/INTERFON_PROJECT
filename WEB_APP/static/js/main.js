
(function ($) {
    "use strict";

    
    /*==================================================================
    [ Validate ]*/
    var input = $('.validate-input .input100');

    $('.validate-form').on('submit',function(){
        var check = true;

        for(var i=0; i<input.length; i++) {
            if(validate(input[i]) == false){
                showValidate(input[i]);
                check=false;
            }
        }

        return check;
    });


    $('.validate-form .input100').each(function(){
        $(this).focus(function(){
           hideValidate(this);
        });
    });

    function validate (input) {
        if($(input).attr('type') == 'email' || $(input).attr('name') == 'email') {
            if($(input).val().trim().match(/^([a-zA-Z0-9_\-\.]+)@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.)|(([a-zA-Z0-9\-]+\.)+))([a-zA-Z]{1,5}|[0-9]{1,3})(\]?)$/) == null) {
                return false;
            }
        }
        else {
            if($(input).val().trim() == ''){
                return false;
            }
        }
    }

    function showValidate(input) {
        var thisAlert = $(input).parent();

        $(thisAlert).addClass('alert-validate');
    }

    function hideValidate(input) {
        var thisAlert = $(input).parent();

        $(thisAlert).removeClass('alert-validate');
    }
    
    

})(jQuery);







//SCRIPTS   

function sendPresetValue(sensorId, value) {
    document.getElementById(`value-display-${sensorId}`).innerText = value; // Update display with preset value
    document.querySelector(`#sensor-${sensorId} .range-slider`).value = value; // Update slider to reflect preset value

    // Send the preset value as an analog command
    fetch(sendCommandUrl, {
        method: "POST",
        headers: { 
            "Content-Type": "application/json", 
            "X-CSRFToken": csrfToken 
        },
        body: JSON.stringify({ sensor_id: sensorId, command: "set_value", value: value })
    })
    .then(response => response.json())
    .then(data => console.log(data.message || "Preset value updated successfully"));
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

function sendAnalogValue(sensorId, value) {
    document.getElementById(`value-display-${sensorId}`).innerText = value;
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

function updateSensorDiv(sensorId, sensorData) {
    const sensorDiv = document.getElementById(`sensor-${sensorId}`);
    if (sensorDiv) {
        const statusElement = sensorDiv.querySelector(".sensor-status");
        if (statusElement) {
            statusElement.textContent = sensorData.status;
        }

        const valueElement = sensorDiv.querySelector(".sensor-value");
        if (valueElement) {
            valueElement.textContent = sensorData.value;
        }

        sensorDiv.classList.remove("bg-blue", "bg-orange");
        if (sensorData.value == 1 && sensorData.name === "PIN_1_BLUE") {
            sensorDiv.classList.add("bg-blue");
        } 
        if (sensorData.value == 1 && sensorData.name === "PIN_2_ORANGE") {
            sensorDiv.classList.add("bg-orange");
        }

        if (sensorData.status == "high" && sensorData.name == "D_LED_ORANGE") {
            sensorDiv.classList.add("bg-orange");
        }

        if (sensorData.status == "high" && sensorData.name === "D_LED_BLUE") {
            sensorDiv.classList.add("bg-blue");
        }

        const alpha = sensorData.value / 255;
        sensorDiv.style.backgroundColor = "";

        if (sensorData.name === "A_LED_VERDE") {
            sensorDiv.style.backgroundColor = `rgba(0, 255, 0, ${alpha})`;
        } else if (sensorData.name === "A_LED_ROSU") {
            sensorDiv.style.backgroundColor = `rgba(255, 0, 0, ${alpha})`;
        }
    }
}

setInterval(fetchSensorData, 1800);