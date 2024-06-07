/**
* @file SMAF-Vanilla-Development-Kit.ino
* @brief Main Arduino sketch for the SMAF-Vanilla-Development-Kit project.
*
* @license MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "WiFi.h"
#include "WiFiConfig.h"
#include "PubSubClient.h"
#include "SensoryAlert.h"
#include "Helpers.h"
#include "Wire.h"

// Define constants for ESP32 core numbers.
#define ESP32_CORE_PRIMARY 0    // Numeric value representing the primary core.
#define ESP32_CORE_SECONDARY 1  // Numeric value representing the secondary core.

// Enum to represent different device statuses.
enum DeviceStatusEnum : byte {
  NONE,             // Disable RGB led.
  NOT_READY,        // Device is not ready.
  READY_TO_SEND,    // Device is ready to send data.
  WAITING_GNSS,     // Device is waiting for GNSS data.
  MAINTENANCE_MODE  // Device is in maintenance mode.
};

// Variable to store the current device status.
DeviceStatusEnum deviceStatus = NONE;  // Initial state is set to NOT_READY.

// Function prototype for the DeviceStatusThread function.
void DeviceStatusThread(void* pvParameters);

// SoftAP configuration parameters.
const char* configNetworkName = "SMAF-DK-SAP-CONFIG";
const char* configNetworkPass = "123456789";
const char* preferencesNamespace = "SMAF-DK";
uint16_t configServerPort = 80;

// WiFiConfig instance with the specified configuration.
WiFiConfig config(configNetworkName,
                  configNetworkPass,
                  configServerPort,
                  preferencesNamespace);

/**
* @brief WiFiClient and PubSubClient instances for establishing MQTT communication.
* 
* The WiFiClient instance, named wifiClient, is used to manage the Wi-Fi connection.
* The PubSubClient instance, named mqtt, relies on the WiFiClient for MQTT communication.
*/
WiFiClient wifiClient;          // Manages Wi-Fi connection.
PubSubClient mqtt(wifiClient);  // Uses WiFiClient for MQTT communication.

/**
* @brief Constructor for SensoryAlert class.
*
* Initializes an instance of the SensoryAlert class with the provided configurations.
* The NeoPixel pin should be set up as OUTPUT before calling this constructor.
*
* @param neoPixelPin The pin connected to the NeoPixel LED strip.
* @param neoPixelCount The number of NeoPixels in the LED strip.
* @param neoPixelBrightness The brightness level of the NeoPixels (0-255).
* @param speakerPin The pin connected to the speaker for audio feedback.
*/
SensoryAlert sensoryAlert(4, 2, 30, 5);

// Define the pin for the configuration button.
int configurationButton = 6;

/**
* @brief Initializes the SMAF-Development-Kit and runs once at the beginning.
*
* This function is responsible for the initial setup of the SMAF-Development-Kit.
* It is executed once when the Arduino board starts or is reset.
* Configuration settings, pin modes, and other one-time setup tasks can be performed here.
*
*/
void setup() {
  // Create a new task (DeviceStatusThread) and assign it to the primary core (ESP32_CORE_PRIMARY).
  xTaskCreatePinnedToCore(
    DeviceStatusThread,    // Function to implement the task.
    "DeviceStatusThread",  // Name of the task.
    8000,                  // Stack size in words.
    NULL,                  // Task input parameter (e.g., delay).
    1,                     // Priority of the task.
    NULL,                  // Task handle.
    ESP32_CORE_SECONDARY   // Core where the task should run.
  );

  // Initialize serial communication at a baud rate of 115200.
  Serial.begin(115200);

  // Set the pin mode for the configuration button to INPUT.
  pinMode(configurationButton, INPUT);

  // Start I2C and initialize BME280 sensor.
  // I2C pins - SDA: 1, SCL: 2
  Wire.begin(1, 2);

  // Initialize visualization library and play intro melody on speaker.
  sensoryAlert.initializeNeoPixel();
  sensoryAlert.playIntroMelody();

  // Print a formatted welcome message with build information.
  String buildVersion = "v0.002";
  String buildDate = "January, 2024.";
  Serial.printf("\n\rSMAF-DEVELOPMENT-KIT, Crafted with love in Europe.\n\rBuild version: %s\n\rBuild date: %s\n\r\n\r", buildVersion.c_str(), buildDate.c_str());

  // Delay for 2400 milliseconds (2.4 seconds).
  delay(2400);

  // Setup hardware Watchdog timer. Bark Bark.
  initWatchdog(30, true);

  // Clear all preferences in namespace.
  // config.clearPreferences();

  // Load Wi-Fi and MQTT configuration preferences.
  config.loadPreferences();

  // Check if SoftAP configuration server should be started.
  if ((digitalRead(configurationButton) == LOW) || (!config.isConfigValid())) {
    // Set device status to Maintenance Mode.
    deviceStatus = MAINTENANCE_MODE;

    // Disable WDT.
    suspendWatchdog();

    // Play configuration melody on speaker.
    sensoryAlert.playConfigurationMelody();

    // Log SoftAP information and start SoftAP configuration server.
    config.startConfig();

    // Render the configuration page in maintenance mode.
    while (true) {
      config.renderConfigPage();
      delay(10);
    }
  } else {
    // Set device status to Not Ready Mode.
    deviceStatus = NOT_READY;

    // Reset WDT.
    resetWatchdog();
  }
}

/**
* @brief Main execution loop for the SMAF-Development-Kit.
*
* This function runs repeatedly in a loop after the initial setup.
* It is the core of your Arduino program, where continuous tasks and operations should be placed.
* Be mindful of keeping the loop efficient and avoiding long blocking operations.
*
*/
String stringTime = String();

void loop() {
  // Attempt to connect to the Wi-Fi network.
  connectToNetwork();

  // Attempt to connect to the MQTT broker.
  connectToMqttBroker();

  // Store MQTT data here.
  String mqttData = String();

  mqttData += "{";
  mqttData += "\"temperature\":";
  // mqttData += String(bme.readTemperature(), 1);
  mqttData += String(0.0, 1);
  mqttData += ",\"temperature_unit\":\"C\",";
  mqttData += "\"humidity\":";
  // mqttData += String(bme.readHumidity(), 1);
  mqttData += String(0.0, 1);
  mqttData += ",\"humidity_unit\":\"%\",";
  mqttData += "\"pressure\":";
  // mqttData += String(bme.readPressure() / 100.0F, 1);
  mqttData += String(0 / 100.0F, 1);
  mqttData += ",\"pressure_unit\":\"hPa\",";
  mqttData += "\"time\":";
  mqttData += "\"unknown\"";
  mqttData += "}";

  debug(LOG, "MQTT data package: '%s'.", mqttData.c_str());

  // debug(SCS, "Device ready to post data, GNSS signal is locked. Data: '%s'.", mqttData.c_str());
  debug(CMD, "Posting data to MQTT broker '%s' on topic '%s'.", config.getMqttServerAddress(), config.getMqttTopic());
  mqtt.publish(config.getMqttTopic(), mqttData.c_str(), true);

  // Delay between data publish.
  delay(1600);

  // Check for incoming data on defined MQTT topic.
  // This is hard core connection check.
  // If no data on topic is received, we are not connected to internet or server and watchdog will reset the device.
  mqtt.loop();
}

/**
* @brief Attempt to connect SMAF-DK to the configured Wi-Fi network.
*
* If SMAF-DK is not connected to the Wi-Fi network, this function tries to establish
* a connection using the settings from the WiFiConfig instance.
*
* @warning This function may delay for extended periods while attempting to connect
* to the Wi-Fi network.
*/
void connectToNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    // Set initial device status.
    deviceStatus = NOT_READY;

    // Disable auto-reconnect and set Wi-Fi mode to station mode.
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);

    // Log an error if not connected to the configured SSID.
    debug(ERR, "Device not connected to '%s'.", config.getNetworkName());

    // Keep attempting to connect until successful.
    while (WiFi.status() != WL_CONNECTED) {
      debug(CMD, "Connecting device to '%s'", config.getNetworkName());

      // Attempt to connect to the Wi-Fi network using configured credentials.
      WiFi.begin(config.getNetworkName(), config.getNetworkPass());
      delay(6400);
    }

    // Log successful connection and set device status.
    debug(SCS, "Device connected to '%s'.", config.getNetworkName());
  }
}

/**
* @brief Attempt to connect to the configured MQTT broker.
*
* If the MQTT client is not connected, this function tries to establish a connection
* to the MQTT broker using the settings from the WiFiConfig instance.
*
* @note Assumes that MQTT configuration parameters (server address, port, client ID,
* username, password) have been previously set in the WiFiConfig instance.
*
* @warning This function may delay for extended periods while attempting to connect
* to the MQTT broker.
*/
void connectToMqttBroker() {
  if (!mqtt.connected()) {
    // Set initial device status.
    deviceStatus = NOT_READY;

    // Set MQTT server and connection parameters.
    mqtt.setServer(config.getMqttServerAddress(), config.getMqttServerPort());
    // mqtt.setKeepAlive(30000);     // To be configured on the settings page.
    // mqtt.setSocketTimeout(4000);  // To be configured on the settings page.
    mqtt.setCallback(serverResponse);

    // Log an error if not connected.
    debug(ERR, "Device not connected to MQTT broker '%s'.", config.getMqttServerAddress());

    // Keep attempting to connect until successful.
    while (!mqtt.connected()) {
      debug(CMD, "Connecting device to MQTT broker '%s'.", config.getMqttServerAddress());

      if (mqtt.connect(config.getMqttClientId(), config.getMqttUsername(), config.getMqttPass())) {
        // Log successful connection and set device status.
        debug(SCS, "Device connected to MQTT broker '%s'.", config.getMqttServerAddress());

        // Subscribe to MQTT topic.
        mqtt.subscribe(config.getMqttTopic());

        //deviceStatus = WAITING_GNSS;
        deviceStatus = READY_TO_SEND;
      } else {
        // Retry after a delay if connection failed.
        delay(4000);
      }
    }
  }
}

void serverResponse(char* topic, byte* payload, unsigned int length) {
  debug(SCS, "Server '%s' responded.", config.getMqttServerAddress());

  // Reset WDT.
  if (deviceStatus != MAINTENANCE_MODE) {
    resetWatchdog();
  }
}

/**
* @brief Thread function for handling device status indications through an RGB LED.
*
* This thread continuously updates the RGB LED status based on the current device status.
* It uses the DeviceStatusEnum values to determine the appropriate LED indication.
*
* @param pvParameters Pointer to task parameters (not used in this function).
*/
void DeviceStatusThread(void* pvParameters) {
  for (;;) {
    // Clear the NeoPixel LED strip.
    sensoryAlert.clearNeoPixel();

    // Update LED status based on the current device status.
    switch (deviceStatus) {
      case NONE:
        // Idle or loading mode.
        sensoryAlert.displayLoadingMode();
        break;

      case NOT_READY:
        // Blink the LED in red to indicate 'NOT_READY' status.
        sensoryAlert.displayNotReadyMode();
        break;

      case READY_TO_SEND:
        // Burst the LED in green to indicate 'READY_TO_SEND' status.
        sensoryAlert.displayReadyToSendMode();
        break;

      case WAITING_GNSS:
        // Blink the LED in blue to indicate 'WAITING_GNSS' status.
        sensoryAlert.displayWaitingGnssMode();
        break;

      case MAINTENANCE_MODE:
        // Blink the LED in purple to indicate 'MAINTENANCE_MODE' status.
        sensoryAlert.displayMaintenanceMode();
        break;
    }
  }
}