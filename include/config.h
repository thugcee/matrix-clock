#pragma once

#include "driver/i2c.h"
#include <MD_Parola.h>
#include <chrono>
#include <cstdint>
#include <string_view>

// GPIO pin for built-in LED
constexpr int CONFIG_BLINK_GPIO = 2;

// NTP and timezone configuration
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr int NTP_UPDATE_INTERVAL_MS = 60 * 60 * 1000; // 1 hour

// TZ string in POSIX form
constexpr char TIMEZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// Intervals and durations
constexpr int STATUS_UPDATE_INTERVAL_SECONDS{5};
constexpr int FORECAST_DISPLAY_TIME_SECONDS{3};
constexpr int FORECAST_CHART_WAIT{3};
constexpr char FORECAST_API_URL[] =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=52.5200&longitude=13.4049&hourly=temperature_2m&forecast_days=2";
constexpr uint8_t FORECAST_HOURS = 16;


// Display hardware/type (enum-like). Replace with the actual enum type.    
constexpr MD_MAX72XX::moduleType_t DISPLAY_HARDWARE_TYPE = MD_MAX72XX::FC16_HW;

// Display configuration
constexpr uint8_t DISPLAY_CLK_PIN = 13;
constexpr uint8_t DISPLAY_DATA_PIN = 14;
constexpr uint8_t DISPLAY_CS_PIN = 12;
constexpr uint8_t DISPLAY_BRIGHTNESS = 0u;
// changing values below is risky without changing the code
constexpr u_int8_t DISPLAY_MAX_DEVICES = 4;
constexpr size_t MATRIX_WIDTH = DISPLAY_MAX_DEVICES * 8;
constexpr size_t MATRIX_HEIGHT = 8;

// MCU Hardware Definitions for Gesture sensor
constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
constexpr gpio_num_t I2C_SDA = GPIO_NUM_21;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_22;
constexpr uint32_t I2C_FREQ_HZ = 100000; // 100 kHz
constexpr gpio_num_t APDS_INT_PIN = GPIO_NUM_4;

static_assert(FORECAST_HOURS <= MATRIX_WIDTH,
              "FORECAST_HOURS must be less than or equal to MATRIX_WIDTH");