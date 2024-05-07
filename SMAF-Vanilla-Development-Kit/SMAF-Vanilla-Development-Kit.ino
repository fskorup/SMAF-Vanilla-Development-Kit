/**
* @file SMAF-Development-Kit.ino
* @brief Main Arduino sketch for the SMAF-Development-Kit project.
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
#include "DeviceStatusVisualizer.h"
#include "PianoNotes.h"
#include "Helpers.h"
#include "Wire.h"
#include "SparkFun_u-blox_GNSS_v3.h"

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
int configurationButton = D2;

// SoftAP configuration parameters.
const char* configNetworkName = "SMAF-DK-SAP-CONFIG";
const char* configNetworkPass = "Kurwe01!";
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

// Instantiate DeviceStatusVisualizer with RGB LED pins: LED_RED, LED_GREEN, LED_BLUE.
DeviceStatusVisualizer statusVisualizer(LED_RED, LED_GREEN, LED_BLUE);

// SFE_UBLOX_GNSS uses I2C.
SFE_UBLOX_GNSS gnss;

/**
* @brief Initializes the SMAF-Development-Kit and runs once at the beginning.
*
* This function is responsible for the initial setup of the SMAF-Development-Kit.
* It is executed once when the Arduino board starts or is reset.
* Configuration settings, pin modes, and other one-time setup tasks can be performed here.
*
*/
void setup() {
  // Initialize serial communication at a baud rate of 115200.
  Serial.begin(115200);

  // Set the pin mode for the configuration button to INPUT.
  pinMode(configurationButton, INPUT);

  // Start I2C.
  Wire.begin();

  // Play intro tone on speaker.
  // TODO: Make a function from this.
  tone(D3, NOTE_E6);
  delay(160);
  noTone(D3);
  tone(D3, NOTE_F6);
  delay(160);
  noTone(D3);
  tone(D3, NOTE_G6);
  delay(320);
  noTone(D3);

  // Delay for 2400 milliseconds (2.4 seconds).
  delay(2400);

  // Print a formatted welcome message with build information.
  String buildVersion = "v0.002";
  String buildDate = "January, 2024.";
  Serial.printf("\n\rSMAF-DEVELOPMENT-KIT, Crafted with love in Europe.\n\rBuild version: %s\n\rBuild date: %s\n\r\n\r", buildVersion.c_str(), buildDate.c_str());

  // Setup hardware Watchdog timer. Bark Bark.
  initWatchdog(30, true);

  // Create a new task (DeviceStatusThread) and assign it to the primary core (ESP32_CORE_PRIMARY).
  xTaskCreatePinnedToCore(
    DeviceStatusThread,    // Function to implement the task.
    "DeviceStatusThread",  // Name of the task.
    8000,                  // Stack size in words.
    NULL,                  // Task input parameter (e.g., delay).
    1,                     // Priority of the task.
    NULL,                  // Task handle.
    ESP32_CORE_PRIMARY     // Core where the task should run.
  );

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
    // Play configuration tone on speaker.
    // TODO: Make a function from this.
    tone(D3, NOTE_F6);
    delay(160);
    noTone(D3);
    tone(D3, NOTE_F6);
    delay(160);
    noTone(D3);
    tone(D3, NOTE_F6);
    delay(320);
    noTone(D3);

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
    // Start GNSS module.
    while (!gnss.begin()) {
      debug(ERR, "GNSS module not detected on I2C lines.");
      delay(800);
    }

    // Log successful GNSS module initialization.
    debug(SCS, "GNSS module detected on I2C lines.");

    // Set the I2C port to output UBX only (turn off NMEA noise).
    gnss.setI2COutput(COM_TYPE_UBX);

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
  }

  // Attempt to connect to the Wi-Fi network.
  connectToNetwork();

  // Attempt to connect to the MQTT broker.
  connectToMqttBroker();

  // Store MQTT data here.
  String mqttData = String();

  // Request (poll) the position, velocity and time (PVT) information.
  // The module only responds when a new position is available. Default is once per second.
  // getPVT() returns true when new data is received.
  if (gnss.getPVT() == true) {
    bool fixStatus = gnss.getGnssFixOk();
    int32_t latitude = gnss.getLatitude();
    int32_t longitude = gnss.getLongitude();
    int32_t speed = gnss.getGroundSpeed();
    int32_t altitude = gnss.getAltitudeMSL();

    // mqttData += "{";
    // mqttData += "\"tme\": \"";
    // mqttData += addLeadingZero(gnss.getHour());
    // mqttData += addLeadingZero(gnss.getMinute());
    // mqttData += addLeadingZero(gnss.getSecond());
    // mqttData += addLeadingZero(gnss.getDay());
    // mqttData += addLeadingZero(gnss.getMonth());
    // mqttData += addLeadingZero(gnss.getYear());
    // mqttData += "\", ";
    // mqttData += "\"lat\": ";
    // mqttData += String((latitude * 1E-7), 6);
    // mqttData += ", ";
    // mqttData += "\"lon\": ";
    // mqttData += String((longitude * 1E-7), 6);
    // mqttData += ", ";
    // mqttData += "\"spd\": ";
    // mqttData += String(int((speed / 1000.0) * 3.6));
    // mqttData += ", ";
    // mqttData += "\"alt\": ";
    // mqttData += String(int(altitude / 1000.0));
    // mqttData += "}";

    mqttData += "{";
    mqttData += "\"o\":";
    mqttData += String((longitude * 1E-7), 6);
    mqttData += ",";
    mqttData += "\"a\":";
    mqttData += String((latitude * 1E-7), 6);
    mqttData += ",";
    mqttData += "\"s\":";
    mqttData += String(int((speed / 1000.0) * 3.6));
    mqttData += ",";
    mqttData += "\"t\":\"";
    mqttData += addLeadingZero(gnss.getHour());
    mqttData += addLeadingZero(gnss.getMinute());
    mqttData += addLeadingZero(gnss.getSecond());
    mqttData += addLeadingZero(gnss.getDay());
    mqttData += addLeadingZero(gnss.getMonth());
    mqttData += "24"; // TODO: gnss.getYear(); Clean this up. Year is returned like YYYY and we need it in YY format.
    mqttData += "\"";
    mqttData += "}";

    //debug(LOG, "MQTT data package: '%s'.", mqttData.c_str());

    // If the device is ready to send, publish a message to the MQTT broker.
    if (fixStatus && latitude != 0 && longitude != 0) {
      deviceStatus = READY_TO_SEND;
      debug(SCS, "Device ready to post data, GNSS signal is locked. Data: '%s'.", mqttData.c_str());
      debug(CMD, "Posting data to MQTT broker '%s' on topic '%s'.", config.getMqttServerAddress(), config.getMqttTopic());
      mqtt.publish(config.getMqttTopic(), mqttData.c_str(), true);
    } else {
      deviceStatus = WAITING_GNSS;
      debug(ERR, "Device is not ready to post data, searching for GNSS signal.");
    }

    // Reset WDT.
    // resetWatchdog();
  }

  mqtt.loop();

  // Delay before repeating the loop.
  // delay(1600);
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

        deviceStatus = WAITING_GNSS;
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
  resetWatchdog();
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
    // Turn off all LEDs before updating the status indication.
    statusVisualizer.shutOffAll();

    // Update LED status based on the current device status.
    switch (deviceStatus) {
      case NONE:
        // No specific indication for 'NONE' status.
        statusVisualizer.shutOffAll();
        break;

      case NOT_READY:
        // Blink the LED in red to indicate 'NOT_READY' status.
        statusVisualizer.blinkRed(240);
        break;

      case READY_TO_SEND:
        // Burst the LED in green to indicate 'READY_TO_SEND' status.
        statusVisualizer.burstGreen(80, 1200, 4);
        break;

      case WAITING_GNSS:
        // Blink the LED in blue to indicate 'WAITING_GNSS' status.
        statusVisualizer.blinkBlue(240);
        break;

      case MAINTENANCE_MODE:
        // Blink the LED in purple to indicate 'MAINTENANCE_MODE' status.
        statusVisualizer.blinkPurple(240);
        break;
    }
  }
}