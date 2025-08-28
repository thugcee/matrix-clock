#pragma once

#include "driver/i2c.h"
#include <cstdint>
#include <chrono>
#include <string_view>
#include <MD_Parola.h>

// GPIO pin for built-in LED
constexpr int CONFIG_BLINK_GPIO = 2;

// TZ string in POSIX form
constexpr char TIMEZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// Intervals and durations
constexpr int STATUS_UPDATE_INTERVAL_SECONDS{5};
constexpr int FORECAST_UPDATE_INTERVAL_SECONDS{15};
constexpr int FORECAST_DISPLAY_TIME_SECONDS{3};

// Display hardware/type (enum-like). Using an int to match MD_MAX72XX::FC16_HW
// Replace with the actual enum type if available:
constexpr MD_MAX72XX::moduleType_t DISPLAY_HARDWARE_TYPE = MD_MAX72XX::FC16_HW;

// Display configuration
constexpr u_int8_t DISPLAY_MAX_DEVICES = 4;
constexpr uint8_t DISPLAY_CLK_PIN = 13;
constexpr uint8_t DISPLAY_DATA_PIN = 14;
constexpr uint8_t DISPLAY_CS_PIN = 12;
constexpr uint8_t DISPLAY_BRIGHTNESS = 0u;

// Forecast hours
constexpr std::size_t FORECAST_HOURS = 16;

// MCU Hardware Definitions for Gesture sensor
constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
constexpr gpio_num_t I2C_SDA = GPIO_NUM_21;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_22;
constexpr uint32_t I2C_FREQ_HZ = 100000; // 100 kHz
constexpr gpio_num_t APDS_INT_PIN = GPIO_NUM_4;
