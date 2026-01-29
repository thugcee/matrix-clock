#pragma once

#include "driver/i2c.h"
#include <MD_Parola.h>
#include <chrono>

// GPIO pin for built-in LED
constexpr int CONFIG_BLINK_GPIO = 2;

// NTP and timezone configuration
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr int NTP_UPDATE_INTERVAL_MS = 60 * 60 * 1000; // 1 hour

// TZ string in POSIX form
constexpr char TIMEZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// Intervals and durations
constexpr uint8_t STATUS_UPDATE_INTERVAL_SECONDS{5};
constexpr uint8_t FORECAST_MINIMAL_DISPLAY_TIME_SECONDS{3};
constexpr uint8_t FORECAST_PAGE_SWITCH_PROXIMITY{12}; // proximity threshold to switch forecast pages
constexpr uint8_t PRECIPITATION_PAGE_SWITCH_PROXIMITY{100}; // proximity threshold for precipitation chart
constexpr char FORECAST_API_URL[] =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=53.428543&longitude=14.552812&hourly=temperature_2m,precipitation_probability&forecast_days=2";
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

// --- Relay / MQTT config ---
constexpr char MQTT_BROKER_IP[] = "BROKER_IP_PLACEHOLDER"; // replace with e.g. "192.168.1.10"
constexpr uint16_t MQTT_BROKER_PORT = 1883;

// Device and MQTT topic base
constexpr char DEVICE_NAME[] = "sew-matrix-clock";
constexpr char MQTT_TOPIC_BASE[] = "clock"; // final topics will be "home/light1/command", etc.

#define ENABLE_MDNS // comment out to disable broadcasting the name via mDNS

// Relay GPIOs (default: 3 relays; add 4th if needed)
constexpr gpio_num_t RELAY_GPIO_1 = GPIO_NUM_25;
constexpr gpio_num_t RELAY_GPIO_2 = GPIO_NUM_26;
constexpr gpio_num_t RELAY_GPIO_3 = GPIO_NUM_27;
// constexpr gpio_num_t RELAY_GPIO_4 = GPIO_NUM_33; // example — change if you add the 4th relay

// Relay active level: true = active HIGH, false = active LOW (common 5V opto-isolated modules are active LOW)
constexpr bool RELAY_ACTIVE_HIGH = false;

// NVS namespace & keys
constexpr char NVS_NAMESPACE[] = "relays";
constexpr char NVS_KEY_FMT[] = "r%u"; // use r1, r2... (sprintf style)

static_assert(FORECAST_HOURS <= MATRIX_WIDTH,
              "FORECAST_HOURS must be less than or equal to MATRIX_WIDTH");