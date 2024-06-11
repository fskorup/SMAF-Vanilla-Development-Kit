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
#include "AudioVisualNotifications.h"
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

// SoftAP configurationuration parameters.
const char* configurationNetworkName = "SMAF-DK-SAP-configuration";
const char* configurationNetworkPass = "123456789";
const char* preferencesNamespace = "SMAF-DK";
uint16_t configurationServerPort = 80;

// WiFiconfiguration instance with the specified configurationuration.
WiFiConfig configuration(configurationNetworkName,
                         configurationNetworkPass,
                         configurationServerPort,
                         preferencesNamespace);

// Preferences variables.
static const char* networkName;
static const char* networkPass;
static const char* mqttServerAddress;
static const char* mqttUsername;
static const char* mqttPass;
static const char* mqttClientId;
static const char* mqttTopic;
static uint16_t mqttServerPort;
static bool audioNotifications;
static bool visualNotifications;

/**
* @brief WiFiClient and PubSubClient instances for establishing MQTT communication.
* 
* The WiFiClient instance, named wifiClient, is used to manage the Wi-Fi connection.
* The PubSubClient instance, named mqtt, relies on the WiFiClient for MQTT communication.
*/
WiFiClient wifiClient;          // Manages Wi-Fi connection.
PubSubClient mqtt(wifiClient);  // Uses WiFiClient for MQTT communication.

/**
* @brief Constructs an instance of the AudioVisualNotifications class.
*
* Initializes an instance of the AudioVisualNotifications class with the provided configurations.
* The NeoPixel pin should be set up as OUTPUT before calling this constructor.
*
* @param neoPixelPin The pin connected to the NeoPixel LED strip.
* @param neoPixelCount The number of NeoPixels in the LED strip.
* @param neoPixelBrightness The brightness level of the NeoPixels (0-255).
* @param speakerPin The pin connected to the speaker for audio feedback.
*/
AudioVisualNotifications notifications(4, 2, 30, 5);

// Define the pin for the configurationuration button.
int configurationurationButton = 6;

/**
* @brief Initializes the SMAF-Development-Kit and runs once at the beginning.
*
* This function is responsible for the initial setup of the SMAF-Development-Kit.
* It is executed once when the Arduino board starts or is reset.
* configurationuration settings, pin modes, and other one-time setup tasks can be performed here.
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

  // Set the pin mode for the configurationuration button to INPUT.
  pinMode(configurationurationButton, INPUT);

  // Start I2C and initialize BME280 sensor.
  Wire.begin(1, 2);

  // Load all preferences to variables.
  networkName = configuration.getNetworkName();
  networkPass = configuration.getNetworkPass();
  mqttServerAddress = configuration.getMqttServerAddress();
  mqttUsername = configuration.getMqttUsername();
  mqttPass = configuration.getMqttPass();
  mqttClientId = configuration.getMqttClientId();
  mqttTopic = configuration.getMqttTopic();
  mqttServerPort = configuration.getMqttServerPort();
  audioNotifications = configuration.getAudioNotificationsStatus();
  visualNotifications = configuration.getVisualNotificationsStatus();

  // Initialize visualization library neo pixels.
  // This does not light up neo pixels.
  notifications.initializeVisualNotifications();

  // Play intro melody on speaker if enabled in preferences.
  if (audioNotifications) {
    notifications.introAudioNotification();
  }

  // Delay for 2400 milliseconds (2.4 seconds).
  delay(1600);

  // Print a formatted welcome message with build information.
  String buildVersion = "v0.002";
  String buildDate = "Q2, 2024.";
  Serial.printf("\n\rSMAF-DEVELOPMENT-KIT, Crafted with love in Europe.\n\rBuild version: %s\n\rBuild date: %s\n\r\n\r", buildVersion, buildDate);

  bool isConfigurationValid = configuration.loadPreferences();

  // Check if SoftAP configuration server should be started.
  if ((digitalRead(configurationurationButton) == LOW) || (!isConfigurationValid)) {
    // Log SoftAP information and start SoftAP configurationuration server.
    configuration.startConfiguration();

    // Set device status to Maintenance Mode.
    deviceStatus = MAINTENANCE_MODE;

    // Play configuration melody notification on speaker.
    if (audioNotifications) {
      notifications.maintenanceAudioNotification();
    }

    // Render the configurationuration page in maintenance mode.
    while (true) {
      configuration.renderConfigurationPage();
    }
  } else {
    // Set device status to Not Ready Mode.
    deviceStatus = NOT_READY;

    // Setup hardware Watchdog timer. Bark Bark.
    initWatchdog(30, true);
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
  debug(CMD, "Posting data to MQTT broker '%s' on topic '%s'.", mqttServerAddress, mqttTopic);
  mqtt.publish(mqttTopic, mqttData.c_str(), true);

  // Delay between data publish.
  delay(1600);

  // Check for incoming data on defined MQTT topic.
  // This is hard core connection check.
  // If no data on topic is received, we are not connected to internet or server and watchdog will reset the device.
  mqtt.loop();
}

/**
* @brief Attempt to connect SMAF-DK to the configurationured Wi-Fi network.
*
* If SMAF-DK is not connected to the Wi-Fi network, this function tries to establish
* a connection using the settings from the WiFiconfiguration instance.
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

    // Log an error if not connected to the configurationured SSID.
    debug(ERR, "Device not connected to '%s'.", networkName);

    // Keep attempting to connect until successful.
    while (WiFi.status() != WL_CONNECTED) {
      debug(CMD, "Connecting device to '%s'", networkName);

      // Attempt to connect to the Wi-Fi network using configurationured credentials.
      WiFi.begin(networkName, networkPass);
      delay(6400);
    }

    // Log successful connection and set device status.
    debug(SCS, "Device connected to '%s'.", networkName);
  }
}

/**
* @brief Attempt to connect to the configurationured MQTT broker.
*
* If the MQTT client is not connected, this function tries to establish a connection
* to the MQTT broker using the settings from the WiFiconfiguration instance.
*
* @note Assumes that MQTT configurationuration parameters (server address, port, client ID,
* username, password) have been previously set in the WiFiconfiguration instance.
*
* @warning This function may delay for extended periods while attempting to connect
* to the MQTT broker.
*/
void connectToMqttBroker() {
  if (!mqtt.connected()) {
    // Set initial device status.
    deviceStatus = NOT_READY;

    // Set MQTT server and connection parameters.
    mqtt.setServer(mqttServerAddress, mqttServerPort);
    // mqtt.setKeepAlive(30000);     // To be configurationured on the settings page.
    // mqtt.setSocketTimeout(4000);  // To be configurationured on the settings page.
    mqtt.setCallback(serverResponse);

    // Log an error if not connected.
    debug(ERR, "Device not connected to MQTT broker '%s'.", mqttServerAddress);

    // Keep attempting to connect until successful.
    while (!mqtt.connected()) {
      debug(CMD, "Connecting device to MQTT broker '%s'.", mqttServerAddress);

      if (mqtt.connect(mqttClientId, mqttUsername, mqttPass)) {
        // Log successful connection and set device status.
        debug(SCS, "Device connected to MQTT broker '%s'.", mqttServerAddress);

        // Subscribe to MQTT topic.
        mqtt.subscribe(mqttTopic);

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
  debug(SCS, "Server '%s' responded.", mqttServerAddress);

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
    notifications.clearAllVisualNotifications();

    if (visualNotifications) {
      // Update LED status based on the current device status.
      switch (deviceStatus) {
        case NONE:
          notifications.loadingVisualNotification();
          break;
        case NOT_READY:
          notifications.notReadyVisualNotification();
          break;
        case READY_TO_SEND:
          notifications.readyToSendVisualNotification();
          break;
        case WAITING_GNSS:
          notifications.waitingGnssFixVisualNotification();
          break;
        case MAINTENANCE_MODE:
          notifications.maintenanceVisualNotification();
          break;
      }
    }
  }
}