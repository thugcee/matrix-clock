
# Firmware for ESP32-based Clock and Weather Forecast

**Used hardware:**

- ESP32 MCU (WiFi required)
- MAX7219 – the main display
- APDS‑9960 – proximity-based switching of display pages

**Used software:**

- PlatformIO for building
- Arduino framework
- ESP-IDF framework with utilization of underlying FreeRTOS

## Features

- Time is synchronized every hour using Simple NTP. This should sufficiently mitigate the imprecision of the built-in RTC.
- Fetches weather forecast from `api.open-meteo.com` every hour.
- Uses a popular 32x8 MAX7219 LED matrix display to show data.
- Uses an APDS‑9960 proximity and gesture sensor to switch displayed pages; only proximity is used for page switching.
- Available pages (from furthest to closest):
  - Current time (`%H:%M`) and day of the week.
  - Weather forecast – minimal and maximal temperature for the next `FORECAST_HOURS`.
  - Weather forecast – temperature chart.
  - Weather forecast – precipitation probability chart.
- Uses both the Arduino and ESP-IDF frameworks, with additional direct calls to the FreeRTOS API.

## Technical info

- This is a PlatformIO project (this may change in the future).
- Both Arduino and ESP-IDF frameworks are used, with many direct calls to FreeRTOS.
- The code is primarily C-style, written using C++17 syntax.

## Setup

Copy `src/secrets.cpp.dist` to `src/secrets.cpp` and fill in your WiFi credentials.

Edit `include/config.h` to configure options, especially the location for the weather forecast.

## Todo

- [x] Nicer fonts – already started, currently based on <https://github.com/mfactory-osaka/ESPTimeCast>.
- [ ] Adapt display brightness depending on ambient brightness.
- [ ] WiFi provisioning with BT app.
- [ ] OTA firmware update.
- [ ] MQTT interface.
- [ ] Remove the Arduino loop and rely on FreeRTOS tasks only (almost done).
- [ ] Remove dependency on the Arduino framework.
