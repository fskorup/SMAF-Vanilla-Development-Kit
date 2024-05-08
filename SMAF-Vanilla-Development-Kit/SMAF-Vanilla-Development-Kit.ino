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
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"

Adafruit_BME280 bme;  // I2C
Adafruit_Sensor* bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor* bme_pressure = bme.getPressureSensor();
Adafruit_Sensor* bme_humidity = bme.getHumiditySensor();

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

// Define the pin for the configuration button.
int configurationButton = 6;

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
SensoryAlert sensoryAlert(4, 2, 60, 5);

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
  bme.begin(0x77);

  // Initialize visualization library and play intro melody on speaker.
  sensoryAlert.initializeNeoPixel();
  sensoryAlert.playIntroMelody();

  // Delay for 2400 milliseconds (2.4 seconds).
  delay(2400);

  // Print a formatted welcome message with build information.
  String buildVersion = "v0.002";
  String buildDate = "January, 2024.";
  Serial.printf("\n\rSMAF-DEVELOPMENT-KIT, Crafted with love in Europe.\n\rBuild version: %s\n\rBuild date: %s\n\r\n\r", buildVersion.c_str(), buildDate.c_str());

  // Setup hardware Watchdog timer. Bark Bark.
  initWatchdog(30, true);

  // Clear all preferences in namespace.
  // config.clearPreferences();

  // Load Wi-Fi and MQTT configuration preferences.
  debug(CMD, "Loading preferences from '%s' namespace.", preferencesNamespace);
  config.loadPreferences();

  // Log preferences information.
  debug(LOG, "Network Name: '%s'.", config.getNetworkName());
  debug(LOG, "Network Password: '%s'.", config.getNetworkPass());
  debug(LOG, "MQTT Server address: '%s'.", config.getMqttServerAddress());
  debug(LOG, "MQTT Server port: '%d'.", config.getMqttServerPort());
  debug(LOG, "MQTT Username: '%s'.", config.getMqttUsername());
  debug(LOG, "MQTT Password: '%s'.", config.getMqttPass());
  debug(LOG, "MQTT Client ID: '%s'.", config.getMqttClientId());
  debug(LOG, "MQTT Topic: '%s'.", config.getMqttTopic());

  // Check if configuration preferences are valid and log the result.
  (!config.isConfigValid()) ? debug(ERR, "Preferences not valid.") : debug(SCS, "Preferences are valid.");

  // Check if SoftAP configuration server should be started.
  if ((digitalRead(configurationButton) == LOW) || (!config.isConfigValid())) {
    // Play configuration melody on speaker.
    sensoryAlert.playConfigurationMelody();

    // Set device status to Maintenance Mode.
    deviceStatus = MAINTENANCE_MODE;

    // Log information about starting SoftAP configuration server.
    debug(CMD, "Initiating the SoftAP configuration server either at the user's request or due to invalid preferences.");
    config.startConfig();
    debug(SCS, "SoftAP configuration server started.");
    debug(LOG, "SoftAP Name: '%s'.", config.getConfigNetworkName());
    debug(LOG, "SoftAP Password: '%s'.", config.getConfigNetworkPass());
    debug(LOG, "SoftAP Server IP address: '%s'.", config.getConfigServerIp());
    debug(LOG, "SoftAP Server port: '%d'.", config.getConfigServerPort());

    // Disable WDT.
    suspendWatchdog();
  } else {
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
void loop() {
  // Render the configuration page in maintenance mode.
  while (deviceStatus == MAINTENANCE_MODE) {
    config.renderConfigPage();
    delay(10);
  }

  // Attempt to connect to the Wi-Fi network.
  connectToNetwork();

  // Attempt to connect to the MQTT broker.
  connectToMqttBroker();

  sensors_event_t temp_event, pressure_event, humidity_event;
  bme_temp->getEvent(&temp_event);
  bme_pressure->getEvent(&pressure_event);
  bme_humidity->getEvent(&humidity_event);

  // Store MQTT data here.
  String mqttData = String();

  mqttData += "{";
  mqttData += "\"temperature\":";
  mqttData += String(temp_event.temperature, 2);
  mqttData += ",\"temperature_unit\":\"C\",";
  mqttData += "\"humidity\":";
  mqttData += String(humidity_event.relative_humidity, 2);
  mqttData += ",\"humidity_unit\":\"hPa\",";
  mqttData += "\"pressure\":";
  mqttData += String(pressure_event.pressure, 2);
  mqttData += ",\"pressure_unit\":\"%\"}";

  debug(LOG, "MQTT data package: '%s'.", mqttData.c_str());

  //debug(SCS, "Device ready to post data, GNSS signal is locked. Data: '%s'.", mqttData.c_str());
  debug(CMD, "Posting data to MQTT broker '%s' on topic '%s'.", config.getMqttServerAddress(), config.getMqttTopic());
  mqtt.publish(config.getMqttTopic(), mqttData.c_str(), true);

  delay(2000);

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
      debug(CMD, "Connecting device to '%s'.", config.getNetworkName());

      // Reset WDT.
      resetWatchdog();

      // Attempt to connect to the Wi-Fi network using configured credentials.
      WiFi.begin(config.getNetworkName(), config.getNetworkPass());
      delay(6400);

      // Uncomment the following line to restart the ESP32 in case of persistent connection issues.
      // esp_restart();
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
    mqtt.setCallback(callback);

    // Log an error if not connected.
    debug(ERR, "Device not connected to MQTT broker '%s'.", config.getMqttServerAddress());

    // Keep attempting to connect until successful.
    while (!mqtt.connected()) {
      debug(CMD, "Connecting device to MQTT broker '%s'.", config.getMqttServerAddress());

      // Reset WDT.
      resetWatchdog();

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

void callback(char* topic, byte* payload, unsigned int length) {
  debug(SCS, "Data posted to MQTT broker '%s' on topic '%s'.", config.getMqttServerAddress(), config.getMqttTopic());

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
  while (true) {
    // Update LED status based on the current device status.
    switch (deviceStatus) {
      case NONE:
        // Clear the NeoPixel LED strip.
        sensoryAlert.clearNeoPixel();
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