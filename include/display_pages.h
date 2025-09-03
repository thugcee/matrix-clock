#pragma once
#include <cstdint>

enum class DisplayPage : uint8_t {
    Time = 0,
    Forecast = 1,
};

enum class ForecastPage : uint8_t {
    None = 0,
    Range = 1,
    Chart = 2,
};
