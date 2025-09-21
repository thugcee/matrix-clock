#pragma once
#include "config.h"
#include "result.h"
#include <WString.h>

template <uint8_t STORED_HOURS>
struct ForecastData {
    float min_temp;
    float max_temp;
    float hourly_temps[STORED_HOURS];
    float precipitation_probability[STORED_HOURS];
    int start_hour;

    using result_t = Result<ForecastData<STORED_HOURS>, String>;
};

using ForecastData16 = ForecastData<FORECAST_HOURS>;
using ForecastResult = ForecastData16::result_t;


// Dynamically scaled temperature chart
template <size_t STORED_HOURS>
std::array<uint8_t, STORED_HOURS> forecast_to_columns(const ForecastData<STORED_HOURS>& f);

// Precipitation probability chart (0-80% scaling) 
template <size_t STORED_HOURS>
std::array<uint8_t, STORED_HOURS> forecast_to_columns_precip(const ForecastData<STORED_HOURS>& f);

template <uint8_t STORED_HOURS>
typename ForecastData<STORED_HOURS>::result_t get_forecast(int start_hour);

void format_temp_range(char *buf, size_t bufsize, float min, float max);

unsigned char reverse_bits_compact(unsigned char b);
