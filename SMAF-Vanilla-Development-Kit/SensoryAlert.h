/**
* @file SensoryAlert.h
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

#ifndef SENSORYALERT_H
#define SENSORYALERT_H

#include "Arduino.h"
#include "Adafruit_NeoPixel.h"
#include "PianoNotes.h"

class SensoryAlert {
public:
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
  SensoryAlert(int neoPixelPin, int neoPixelCount, int neoPixelBrightness, int speakerPin);

  /**
  * @brief Initializes the NeoPixel LED strip.
  *
  * This function initializes the NeoPixel LED strip with the specified pin and settings
  * provided during the construction of the SensoryAlert object.
  * It should be called once at the beginning of the program or whenever the NeoPixel strip needs to be re-initialized.
  */
  void initializeNeoPixel();

  /**
  * @brief Clears the NeoPixel LED strip.
  *
  * This function turns off all NeoPixels in the LED strip, effectively clearing any previous colors or patterns.
  * It can be used to reset the NeoPixel strip to its default state or turn off any active visual feedback.
  */
  void clearNeoPixel();

  /**
  * @brief Plays the introductory melody.
  *
  * This function plays an introductory melody indicating the start of the device operation.
  * It can be used to provide an audible cue when the device is powered on or initialized.
  */
  void playIntroMelody();

  /**
  * @brief Plays the configuration melody.
  *
  * This function plays a melody indicating the device is in configuration mode.
  * It should be called when the device is in configuration mode to provide audible feedback.
  */
  void playConfigurationMelody();

  /**
  * @brief Sets the device to the "not ready" mode.
  *
  * This function sets the device to the "not ready" mode, indicating that it is not yet prepared for operation.
  * It can be used as part of the device initialization process or when certain conditions are not met for operation.
  */
  void displayNotReadyMode();

  /**
  * @brief Sets the device to the "ready to send" mode.
  *
  * This function sets the device to the "ready to send" mode, indicating that it is prepared to send data or perform its main function.
  * It can be used to indicate that the device has completed its initialization process and is ready for operation.
  */
  void displayReadyToSendMode();

  /**
  * @brief Sets the device to the "waiting for GNSS" mode.
  *
  * This function sets the device to the "waiting for GNSS" mode, indicating that it is waiting to acquire a GNSS (Global Navigation Satellite System) signal.
  * It can be used in applications where GNSS data is required for operation and the device is waiting for a valid signal.
  */
  void displayWaitingGnssMode();

  /**
  * @brief Sets the device to the maintenance mode.
  *
  * This function sets the device to the maintenance mode, indicating that it is undergoing maintenance or configuration changes.
  * It can be used to temporarily disable normal operation and perform maintenance tasks on the device.
  */
  void displayMaintenanceMode();


private:
  int _neoPixelPin;
  int _neoPixelCount;
  int _neoPixelBrightness;
  int _speakerPin;
  Adafruit_NeoPixel _neoPixel;  // Declare neoPixel as a member variable
};

#endif