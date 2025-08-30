# Firmware for a digital clock based on ESP32 and MAX7219 display, with time synchronization via NTP and a simple weather forecast.

This is a PlatformIO project that utilises the Arduino and ESP-IDF frameworks.

## Features

- The time is displayed on 32x8 MAX7219 display.
- It connects to WiFi and uses NTP to synchronise real time.
- Time is resynchronised every 1h. This should sufficiently mitigate the imprecise operation of the built-in RTC.
- It uses APDS-9960 proximity and gesture sensor to switch from displaying time to displaying weather forecast (minimal and maximal temperature for the next `FORECAST_HOURS` is shown).
- Weather forecast is downloaded from `api.open-meteo.com` every hour.
- If you hold your hand for a few (at least `FORECAST_CHART_WAIT`) seconds, it will show a temperature chart.
- I try to make it as low power as possible without making it unreliable or too complicated.

## Technical info

- It's a PlatformIO project (this can change in the future).
- Both Arduino and ESP-IDF frameworks are used, but there is also a lot of direct calling to FreeRTOS.
- Mostly C level code in C++ syntax.

## Setup

Copy `src/secrets.cpp.dist` → `src/secrets.cpp` and fill in your WiFi credentials.

Edit `include/config.h` to configure options, especially location for weather forecast.

## Todo

- Add nicer fonts.
- Add WiFi provisioning with BT app.
- Add support for real gestures, not just proximity.
- Remove the Arduino loop and relay on FreeRTOS tasks only (almost done).
- Remove dependency on Arduino framework.
