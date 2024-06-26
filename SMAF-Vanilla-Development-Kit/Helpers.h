/**
* @file Helpers.h
* @brief Declaration of helper functions for Arduino project.
*
* This file contains the declarations of helper functions used in the Arduino project.
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

#ifndef HELPERS_H
#define HELPERS_H

#include "Arduino.h"
#include "esp_task_wdt.h"
#include "esp_system.h"

// Define a macro for comparing version numbers
#define VERSION_CHECK(major, minor, patch) ((major)*10000 + (minor)*100 + (patch))

/**
* @enum messageTypeEnum
* @brief Enumeration for message types used in the project.
*
* This enumeration defines different message types for logging purposes.
*/
enum MessageTypeEnum : byte {
  LOG,  // Info type. INFO message type displayed.
  ERR,  // Error type. ERROR message type displayed.
  SCS,  // Success type. OK message type displayed.
  CMD   // Command type. CMD message type displayed.
};

extern MessageTypeEnum messageType;  // Declare the variable.

/**
* @brief Debugging function to print messages with different types.
*
* This function prints debug messages to the Serial monitor with a specified message type.
*
* @param messageType The type of the message (CLR, ERR, SCS, CMD).
* @param message The message to be printed.
*/
void debug(MessageTypeEnum messageType, const char *format, ...);

/**
* @brief Initializes the ESP32 Watchdog Timer with specified timeout and panic behavior.
*
* This function initializes the ESP32 Watchdog Timer with the provided timeout duration
* and panic behavior. The Watchdog Timer is used to monitor the execution of the program
* and reset the ESP32 in case of a thread or system failure.
*
* @param timeout The timeout duration for the Watchdog Timer in milliseconds.
* @param panic If set to true, enables panic behavior, causing the ESP32 to restart
*              when a timeout occurs. If set to false, the Watchdog Timer triggers a
*              reset without causing a restart.
*/
void initWatchdog(uint32_t timeout, bool panic);

/**
* @brief Resets the ESP32 Watchdog Timer.
*
* This function resets the ESP32 Watchdog Timer, preventing it from triggering a
* reset due to a timeout. It is typically used to indicate that the program is
* still executing as expected and to avoid unintended system resets.
*
* @note It is recommended to reset the Watchdog Timer periodically within the
*       program to prevent it from timing out and triggering a system reset.
*/
void resetWatchdog();

/**
* @brief Suspends the ESP32 Watchdog Timer.
*
* This function disables the ESP32 Watchdog Timer, temporarily suspending its monitoring
* of the program execution. It is useful in scenarios where certain operations or tasks
* may take longer than the Watchdog Timer's timeout duration and need to be exempt from
* triggering a watchdog reset.
*
* @note It is important to resume the Watchdog Timer using resumeWatchdog() after
*       completing the critical operations to avoid unintended system resets.
*/
void suspendWatchdog();

/**
* @brief Check if a C-style string is empty.
* 
* @param str Pointer to the C-style string to check.
* 
* @return true if the string is empty (either a null pointer or the first character is '\0'), false otherwise.
*/
bool isEmpty(const char* str);

/**
* @brief Encloses the given string within double quotes.
*
* This function takes a string `data` and returns it enclosed within double quotes.
*
* @param data The string to be enclosed within double quotes.
* @return A String containing the input `data` enclosed within double quotes.
*/
String quotation(String data);

#endif