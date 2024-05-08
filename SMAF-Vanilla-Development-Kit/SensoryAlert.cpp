/**
* @file SensoryAlert.cpp
* @brief Implementation of the SensoryAlert library for RGB LED and audio status indication.
*
* This file contains the implementation for the SensoryAlert library, which facilitates
* the indication of device status through an NEO-PIXEL LED. The library provides separate functions
* to control the LED for displaying status in terms of red, green, and blue colors. It is designed
* to be easily integrated into Arduino projects for visualizing various device states.
*
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

#include "Arduino.h"
#include "SensoryAlert.h"
#include "Adafruit_NeoPixel.h"
#include "PianoNotes.h"

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
SensoryAlert::SensoryAlert(int neoPixelPin, int neoPixelCount, int neoPixelBrightness, int speakerPin)
  : _neoPixelPin(neoPixelPin),
    _neoPixelCount(neoPixelCount),
    _neoPixelBrightness(neoPixelBrightness),
    _speakerPin(speakerPin),
    _neoPixel(neoPixelCount, neoPixelPin, NEO_GRB + NEO_KHZ800) {
}

/**
* @brief Initializes the NeoPixel LED strip.
*
* This function initializes the NeoPixel LED strip with the specified pin and settings
* provided during the construction of the SensoryAlert object.
* It should be called once at the beginning of the program or whenever the NeoPixel strip needs to be re-initialized.
*/
void SensoryAlert::initializeNeoPixel() {
  _neoPixel.begin();                             // INITIALIZE NeoPixel strip object (REQUIRED)
  _neoPixel.setBrightness(_neoPixelBrightness);  // Set BRIGHTNESS to about 1/5 (max = 255)
}

/**
* @brief Clears the NeoPixel LED strip.
*
* This function turns off all NeoPixels in the LED strip, effectively clearing any previous colors or patterns.
* It can be used to reset the NeoPixel strip to its default state or turn off any active visual feedback.
*/
void SensoryAlert::clearNeoPixel() {
  _neoPixel.clear();
  _neoPixel.show();
}

/**
* @brief Plays the introductory melody.
*
* This function plays an introductory melody indicating the start of the device operation.
* It can be used to provide an audible cue when the device is powered on or initialized.
*/
void SensoryAlert::playIntroMelody() {
  tone(_speakerPin, NOTE_E6);
  delay(160);
  noTone(_speakerPin);

  tone(_speakerPin, NOTE_F6);
  delay(160);
  noTone(_speakerPin);

  tone(_speakerPin, NOTE_G6);
  delay(320);
  noTone(_speakerPin);
}

/**
* @brief Plays the configuration melody.
*
* This function plays a melody indicating the device is in configuration mode.
* It should be called when the device is in configuration mode to provide audible feedback.
*/
void SensoryAlert::playConfigurationMelody() {
  tone(_speakerPin, NOTE_E6);
  delay(1600);
  noTone(_speakerPin);
}

/**
* @brief Sets the device to the "not ready" mode.
*
* This function sets the device to the "not ready" mode, indicating that it is not yet prepared for operation.
* It can be used as part of the device initialization process or when certain conditions are not met for operation.
*/
void SensoryAlert::displayNotReadyMode() {
  uint32_t interval = 240;

  _neoPixel.setPixelColor(0, _neoPixel.Color(255, 0, 0));
  _neoPixel.setPixelColor(1, _neoPixel.Color(0, 0, 0));
  _neoPixel.show();

  delay(interval);

  _neoPixel.setPixelColor(0, _neoPixel.Color(0, 0, 0));
  _neoPixel.setPixelColor(1, _neoPixel.Color(255, 0, 0));
  _neoPixel.show();

  // clearNeoPixel();
  delay(interval);
}

/**
* @brief Sets the device to the "ready to send" mode.
*
* This function sets the device to the "ready to send" mode, indicating that it is prepared to send data or perform its main function.
* It can be used to indicate that the device has completed its initialization process and is ready for operation.
*/
void SensoryAlert::displayReadyToSendMode() {
  uint32_t delayBeforeNextBurst = 1200;
  int blinkCount = 4;

  // Loop through the specified number of blinks in one burst.
  for (int i = 0; i < blinkCount; ++i) {
    _neoPixel.setPixelColor(0, _neoPixel.Color(0, 255, 0));
    _neoPixel.setPixelColor(1, _neoPixel.Color(0, 255, 0));
    _neoPixel.show();

    delay(40);
    clearNeoPixel();
    delay(40);
  }

  // Add a delay before starting the next burst.
  delay(delayBeforeNextBurst);
}

/**
* @brief Sets the device to the "waiting for GNSS" mode.
*
* This function sets the device to the "waiting for GNSS" mode, indicating that it is waiting to acquire a GNSS (Global Navigation Satellite System) signal.
* It can be used in applications where GNSS data is required for operation and the device is waiting for a valid signal.
*/
void SensoryAlert::displayWaitingGnssMode() {
  uint32_t interval = 240;

  _neoPixel.setPixelColor(0, _neoPixel.Color(0, 0, 255));
  _neoPixel.setPixelColor(1, _neoPixel.Color(0, 0, 255));
  _neoPixel.show();

  delay(interval);
  clearNeoPixel();
  delay(interval);
}

/**
* @brief Sets the device to the maintenance mode.
*
* This function sets the device to the maintenance mode, indicating that it is undergoing maintenance or configuration changes.
* It can be used to temporarily disable normal operation and perform maintenance tasks on the device.
*/
void SensoryAlert::displayMaintenanceMode() {
  uint32_t interval = 240;

  _neoPixel.setPixelColor(0, _neoPixel.Color(255, 0, 255));
  _neoPixel.setPixelColor(1, _neoPixel.Color(255, 0, 255));
  _neoPixel.show();

  delay(interval);
  clearNeoPixel();
  delay(interval);
}