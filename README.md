# Esp32 firmware for clock with NTP sync

This is a PlatformIO project that utilizes the Arduino and ESP-IDF frameworks.

It connects to WiFi and uses NTP to sync real time.
Time is resynchronised every 1h.
It should sufficiently mitigate the imprecise operation of the built-in RTC.
The time is displayed on 32x8 MAX-7219 display.

## Setup

Copy `src/secrets.cpp.dist` → `src/secrets.cpp` and fill in your WiFi credentials.

## Todo

- Add weather forecast.
- Add support for gestures.
- Remove the Arduino loop and relay on FreeRTOS tasks only.