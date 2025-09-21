#pragma once
#include <cstdint>

enum class DisplayPage : uint8_t {
    Time = 0,
    Forecast = 1,
};

enum class ForecastPage : uint8_t {
    None = 0,
    TemperatureRange = 1,
    TemperatureChart = 2,
    PrecipitationChart = 3,
};
