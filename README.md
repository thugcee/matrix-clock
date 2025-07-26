# Firmware for a digital clock based on ESP32 and MAX7219 display, with time synchronization via NTP

This is a PlatformIO project that utilises the Arduino and ESP-IDF frameworks.

## Features

- It connects to WiFi and uses NTP to sync real time.
- Time is resynchronised every 1h. This should sufficiently mitigate the imprecise operation of the built-in RTC.
- The time is displayed on 32x8 MAX7219 display.
- I try to make it as low power as possible without making it unreliable or too complicated.

## Setup

Copy `src/secrets.cpp.dist` → `src/secrets.cpp` and fill in your WiFi credentials.

## Todo

- Add weather forecast.
- Add support for gestures.
- Remove the Arduino loop and relay on FreeRTOS tasks only.
