// Signal K & SensESP fan control 

#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_app_builder.h"
#include <Adafruit_INA260.h>
#include <sensesp/transforms/moving_average.h>
#include <sensesp/net/http_server.h>
#include <WebServer.h>
#include <math.h>

using namespace sensesp;


// Fan state variables
int currentFanSpeed = 0; // 0 = off, 1 = 50%, 2 = 70%, 3 = 100%
int currentFanTimer = 0;
const int8_t fanButton=19;
const uint8_t kAnalogInputPin = 35;
const unsigned int kAnalogInputReadInterval = 10000;  // Define how often (in milliseconds) new samples are acquired
const float kAnalogInputScale = 12;    // Set the scale 12 means base of 12v input voltage.
int fanState =  0;
int speedSet = 0;

/////////////////////////////////////////
// Create an instance of the sensor using its I2C interface
/////////////////////////////////////////
Adafruit_INA260 ina260 = Adafruit_INA260();   // Power measurement breakout board


////////////////////////////////////////////
//Here you put all the functions you will need to call within the setup()
////////////////////////////////////////////

float read_power_callback() { 
    float pwr = 0;
    for (int i=0;i<5;i++) {
        pwr += ina260.readPower() / 1000;
        delay(10);
    }
    pwr = (pwr /4)-.4;  // average out the power and adjust by -.4 compared to bench supply
    return (pwr); }

void fanButtonPress() {
        digitalWrite(fanButton, HIGH);
        delay(500);
        digitalWrite(fanButton, LOW);
}


float readFanSpeed() {
    float power = 0;
    power = read_power_callback();
    if (power < 2.3) {fanState =  0;} 
      else if (power < 4) {fanState = 1;}
      else if (power < 5.5) {fanState = 2;}
      else {fanState = 3;}

    return (fanState);
}

void setFanSpeed(int speedSetting) {
    int attempts = 0;
    int presses = 0;
    fanState = readFanSpeed(); 
    
    // Calculate presses needed
    if (speedSetting >= fanState) {
        presses = speedSetting - fanState; // Increases
    } else {
        presses = 4 - fanState + speedSetting; // Wrap around
    }

    // Perform button presses to reach desired speed
    while (presses > 0) {
        fanButtonPress();
        presses--;
        delay(200); // Delay between button presses
    }
}

int setFanTimer() {
    // This will be a place to set the timer function
}

// Create a WebServer instance
WebServer server(8081); // Create a web server on port 8081

// Create HTML content
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Fan Speed Control</title>
    <style>
        body { font-family: Arial; }
        h1 { text-align: center; }
        .button { display: block; margin: 10px auto; padding: 10px 20px; font-size: 20px; }
        .status { text-align: center; font-size: 18px; margin: 10px; }
    </style>
</head>
<body>
    <h1>Fan Speed Control</h1>

    <button class="button" onclick="setSpeed(4)">Fan Speed Button</button>
    <button class="button" onclick="setSpeed(0)">Turn Off</button>
    <button class="button" onclick="setSpeed(1)">Low Speed</button>
    <button class="button" onclick="setSpeed(2)">Medium Speed</button>
    <button class="button" onclick="setSpeed(3)">High Speed</button>


    <script>
        function setSpeed(speedIndex) {
            fetch('/setFanSpeed', {
                method: 'PUT',
                body: JSON.stringify(speedIndex), // Send the speed index
                headers: { 'Content-Type': 'application/json' }
            })
            .then(response => response.text()) // Get the response as text
            .then(data => console.log(data)); // Log the response for debugging
        }

        // function updateStatus() {
        //     fetch('/status')
        //         .then(response => response.json()) // Expect JSON response
        //         .then(data => {
        //             // Update HTML elements with values
        //             document.getElementById('currentSpeed').innerText = "Current Speed: " + data.currentSpeed;
        //             document.getElementById('currentRPM').innerText = "Current RPM: " + data.currentRPM;
        //             document.getElementById('currentTemp').innerText = "Temperature: " + (data.currentTemp).toFixed(2) + " °C"; // Display in °C
        //             document.getElementById('currentPower').innerText = "Power: " + data.currentPower + "W";
        //         });
        // }

        // // Update the status every second
        // setInterval(updateStatus, 1000);
    </script>
</body>
</html>
)rawliteral";

// Handle the root request to show the HTML page
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

void handlePutFanSpeed() {
    if (server.method() == HTTP_PUT) {
        String body = server.arg("plain"); // Get the body of the PUT request
        body.trim(); // Trim whitespace from the body
        debugD("Raw body received: '%s'", body.c_str()); // Log raw body for debugging
        int newSpeed = body.toInt(); // Convert to integer

        // Prepare a response message
        String responseMessage;

        // Add the received value to the response
        responseMessage += "Received PUT request with requested speed: '" + body + "'\n";
        debugD("Conversion result: %d", newSpeed); // Log the integer result

        // Check if the new speed is within a valid range
        if (newSpeed >= 0 && newSpeed < 5) {
            if (newSpeed == 4) {fanButtonPress();}
            else {
            speedSet = newSpeed; // Update the current speed index
            setFanSpeed(newSpeed); // Set the fan speed
            // 

            // Log updated information in response
            responseMessage += "Fan speed updated to index: " + String(currentFanSpeed) +
                               " (Duty Cycle: "; //+ String(fanSpeeds[currentFanSpeed]) + "%)\n";

            server.send(200, "text/plain", responseMessage); // Send back success
        }} else {
            responseMessage += "Invalid speed value: " + String(newSpeed) +
                               ". Must be between 0 and ";// + String(sizeof(fanSpeeds) / sizeof(fanSpeeds[0]) - 1) + ".\n";

            server.send(400, "text/plain", responseMessage); // Send back error for invalid speed
        }
    }
}

//////////////////////////////////////////////////////////////////
// The setup function performs one-time application initialization.
//////////////////////////////////////////////////////////////////

void setup() {
  SetupLogging();

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname("caframoFanControl")
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    // ->set_wifi("KBL", "blacksmith")
                    //->set_sk_server("192.168.10.3", 80)
                    ->enable_ota("transport")
                    ->get_app();


// GPIO number to use for the analog input


  // Create a new Analog Input Sensor that reads an analog input pin
  // periodically.
//   auto* analog_input = new AnalogInput(
//       kAnalogInputPin, kAnalogInputReadInterval, "", kAnalogInputScale);
  ina260.begin();
  unsigned int read_interval_ina = 10000;
  auto* fan_control_power =
      new RepeatSensor<float>(read_interval_ina, read_power_callback);

//   // Add an observer that prints out the current value of the analog input
//   // every time it changes.
//   analog_input->attach([analog_input]() {
//     debugD("Analog input value: %f", analog_input->get());
//   });



//
//   const unsigned int kDigitalOutputInterval = 5650;
pinMode(fanButton, OUTPUT);


//   SensESPBaseApp::get_event_loop()->onRepeat(
//       kDigitalOutputInterval, fanButtonPress);


//   const unsigned int kDigitalOutputInterval = 5650;
//   auto* setFanSpeed =
//       new RepeatSensor<void>(kDigitalOutputInterval, fanButtonPress );



// Read the temperature
//   auto* int_temp =
//       new RepeatSensor<float>(read_interval_emc, read_int_temp );

// Read the voltage
//   auto* currentVoltage = 
//       new RepeatSensor<float>(2000, readVoltage );
  
// Create a Sensor for sharing the variable currentFanSpeed
// auto* speedSettingSensor = new RepeatSensor<int>(500, []() { return currentFanSpeed; });


// Create a Sensor for reading the voltage from the ads1115
// auto* currentVoltage = new RepeatSensor<int>(500, []() { return (ads1115.readADC_SingleEnded(0)); });


/////////////////////////////////////////////
// Set up the web server
//////////////////////////////////////////////

// Register the handler for the root URL to serve the HTML page
server.on("/", HTTP_GET, handleRoot);

// Register the handler for the PUT request to change fan speed
server.on("/setFanSpeed", HTTP_PUT, handlePutFanSpeed);

// Register the handler for the GET request to fetch current status
// server.on("/status", HTTP_GET, handleGetStatus);

// Start the server
server.begin();
debugD("HTTP server started on port 8081");


  ////////////////////////////////////////////
  //  These are all the readings being sent to SignalK
  ////////////////////////////////////////////


// Connect the analog input to Signal K output. This will publish the
// analog input value to the Signal K server every time it changes.
//   analog_input
//   ->connect_to(new MovingAverage(15, 1.0,
//       "/Sensors/StbFan/Power/avg"))
//   ->connect_to(new SKOutputFloat(
//       "sensors.analog_input.voltage",         // Signal K path
//       "/Sensors/Analog Input/Voltage",        // configuration path, used in the
//                                               // web UI and for storing the
//                                               // configuration
//       new SKMetadata("V",                     // Define output units
//                      "Analog input voltage")  // Value description
//       ));

 fan_control_power
    //  ->connect_to(new MovingAverage(5, 1.0,
    //   "/Sensors/StbFan/Power/avg"))
     ->connect_to(new SKOutputFloat(
      "sensors.caframo/power",                 // Signal K path
      "/Sensors/caframo/power",                 // configuration path, used in the
                                               // web UI and for storing the
                                               // configuration
       new SKMetadata("W",                     // Define output units
                     "Caframo Power")          // Value description
      ));



//   fan_tach
//      ->connect_to(new SKOutputInt(
//       "sensors.stb_fan_tach",                  // Signal K path
//       "/Sensors/StbFan/RPM",                   // configuration path, used in the
//                                                // web UI and for storing the
//                                                // configuration
 

}

void loop() { 
    server.handleClient(); // Handle incoming client requests
    SensESPBaseApp::get_event_loop()->tick(); 
    }