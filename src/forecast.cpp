#include <WiFi.h>
#include "forecast.h"
#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WString.h>
#include <icons.h>

unsigned char reverse_bits_compact(unsigned char b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4; // Swap nibbles
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2; // Swap pairs
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1; // Swap bits
    return b;
}

// Precipitation probability chart (0-80% scaling)
template <size_t STORED_HOURS>
std::array<uint8_t, STORED_HOURS> forecast_to_columns_precip(const ForecastData<STORED_HOURS>& f) {
    constexpr float min_precip = 0.0f;
    constexpr float max_precip = 80.0f;
    constexpr float range = max_precip - min_precip;
    constexpr float scale = (MATRIX_HEIGHT - 1) / range;
    std::array<uint8_t, STORED_HOURS> cols{};
    cols.fill(0);
    for (size_t i = 0; i < STORED_HOURS; ++i) {
        float p = f.precipitation_probability[i];
        if (p < min_precip) p = min_precip;
        if (p > max_precip) p = max_precip;
        float pos = (p - min_precip) * scale;
        int row = static_cast<int>(std::lround(pos));
        if (row < 0) row = 0;
        if (row >= static_cast<int>(MATRIX_HEIGHT)) row = MATRIX_HEIGHT - 1;
        uint8_t bits = static_cast<uint8_t>((1u << (row + 1)) - 1u);
        cols[i] = bits;
    }
    return cols;
}
template std::array<uint8_t, FORECAST_HOURS>
forecast_to_columns_precip<FORECAST_HOURS>(const ForecastData<FORECAST_HOURS>& f);

template <size_t STORED_HOURS>
std::array<uint8_t, STORED_HOURS> forecast_to_columns(const ForecastData<STORED_HOURS>& f) {
    std::array<uint8_t, STORED_HOURS> cols{};
    cols.fill(0);

    // Determine min/max range to use: prefer provided min_temp/max_temp if valid (min < max)
    float min_temp = f.min_temp;
    float max_temp = f.max_temp;
    if (!(min_temp < max_temp)) {
        // compute from all samples
        min_temp = f.hourly_temps[0];
        max_temp = f.hourly_temps[0];
        for (size_t i = 1; i < STORED_HOURS; ++i) {
            if (f.hourly_temps[i] < min_temp)
                min_temp = f.hourly_temps[i];
            if (f.hourly_temps[i] > max_temp)
                max_temp = f.hourly_temps[i];
        }
        if (min_temp == max_temp) { // flat line: make a tiny range
            min_temp -= 0.5f;
            max_temp += 0.5f;
        }
    }

    const float range = max_temp - min_temp;
    const float inv_range = (range > 0.0f) ? (1.0f / range) : 0.0f;
    const float scale = (MATRIX_HEIGHT - 1) * inv_range; // maps delta to row index span 0..7

    for (size_t i = 0; i < STORED_HOURS; ++i) {
        float t = f.hourly_temps[i];
        // clamp
        if (t < min_temp)
            t = min_temp;
        else if (t > max_temp)
            t = max_temp;
        // normalized position 0..(MATRIX_HEIGHT-1)
        float pos = (t - min_temp) * scale;
        int row = static_cast<int>(std::lround(pos));
        if (row < 0)
            row = 0;
        else if (row >= static_cast<int>(MATRIX_HEIGHT))
            row = MATRIX_HEIGHT - 1;

        // bits 0..row set: (1u << (row+1)) - 1  (row in [0..7])
        uint8_t bits = static_cast<uint8_t>((1u << (row + 1)) - 1u);
        cols[i] = bits;
    }

    // remaining columns stay zero
    return cols;
}
template std::array<uint8_t, FORECAST_HOURS>
forecast_to_columns<FORECAST_HOURS>(const ForecastData<FORECAST_HOURS>& f);

template <uint8_t STORED_HOURS>
void format_temp_at_hour(char* buf, size_t bufsize, ForecastData<STORED_HOURS>& data, int hour) {
    if (!buf || bufsize == 0)
        return;
    int index = hour - data.start_hour;
    if (index < 0)
        index = 0;
    if (index >= STORED_HOURS)
        index = STORED_HOURS - 1;

    int temp_i = (int)roundf(data.hourly_temps[index]);
    snprintf(buf, bufsize, "%02d:%d", hour, temp_i);
}
template void format_temp_at_hour<FORECAST_HOURS>(char* buf, size_t bufsize,
                                                  ForecastData<FORECAST_HOURS>& data, int hour);

void format_temp_range(char* buf, size_t bufsize, float min, float max) {
    if (!buf || bufsize == 0)
        return;
    if (min == 0.f && max == 0.f) {
        // no data
        snprintf(buf, bufsize, "NoData");
        return;
    }

    int min_i = (int)round(min);
    int max_i = (int)round(max);
    char format[] = "%d-%d ";
    format[sizeof(format)-2] = Icons::DEG_C_CODE;
    snprintf(buf, bufsize, format, min_i, max_i);
}

template <uint8_t STORED_HOURS>
typename ForecastData<STORED_HOURS>::result_t get_forecast(int start_hour) {
    using ForecastResult = typename ForecastData<STORED_HOURS>::result_t;

    if (WiFi.status() != WL_CONNECTED) {
        return ForecastResult::Err("Error: WiFi not connected.");
    }

    if (start_hour + STORED_HOURS > 48) {
        return ForecastResult::Err("Error: Requested hours exceed available forecast range.");
    }

    HTTPClient http;
    
    // Start the HTTP request
    http.begin(String(FORECAST_API_URL));
    int http_code = http.GET();

    // --- HTTP Response Handling ---
    if (http_code > 0) {
        if (http_code == HTTP_CODE_OK) {
            String payload = http.getString();

            // --- JSON Parsing and Data Processing (for ArduinoJson v7.x) ---
            JsonDocument doc; // Use JsonDocument for v7. It handles its own memory.
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                http.end();
                // Return a more specific JSON error
                return ForecastResult::Err("Error: JSON deserialization failed: " +
                                           String(error.c_str()));
            }

            // Check if the expected data is present
            if (!doc["hourly"].is<JsonObject>() ||
                !doc["hourly"]["temperature_2m"].is<JsonArray>() ||
                !doc["hourly"]["precipitation_probability"].is<JsonArray>()) {
                http.end();
                return ForecastResult::Err("Error: Invalid JSON format from API.");
            }

            JsonArray temps = doc["hourly"]["temperature_2m"].as<JsonArray>();
            JsonArray precs = doc["hourly"]["precipitation_probability"].as<JsonArray>();

            if (temps.isNull() || precs.isNull() ||
                temps.size() <= start_hour + STORED_HOURS ||
                precs.size() <= start_hour + STORED_HOURS) {
                http.end();
                return ForecastResult::Err("Error: Not enough forecast data available.");
            }

            ForecastData<STORED_HOURS> forecast_data;
            forecast_data.start_hour = start_hour;
            forecast_data.min_temp = temps[start_hour].as<float>();
            forecast_data.max_temp = temps[start_hour].as<float>();
            for (int i = start_hour; i < temps.size() && i < start_hour + FORECAST_HOURS; i++) {
                float current_temp = temps[i].as<float>();
                float current_prec = precs[i].as<float>();
                Serial.printf("Hour %02d: Temp = %.2f, Prec = %.2f\n", i >= 24 ? i - 24 : i, current_temp, current_prec);
                if (current_temp < forecast_data.min_temp) {
                    forecast_data.min_temp = current_temp;
                }
                if (current_temp > forecast_data.max_temp) {
                    forecast_data.max_temp = current_temp;
                }
                forecast_data.hourly_temps[i - start_hour] = current_temp;
                forecast_data.precipitation_probability[i - start_hour] = current_prec;
            }

            http.end(); // Free resources
            return ForecastResult::Ok(forecast_data);

        } else {
            http.end();
            // Provide a more descriptive HTTP error message
            return ForecastResult::Err("Error: HTTP request failed, code: " + String(http_code));
        }
    } else {
        http.end();
        // Provide a more descriptive client-side error
        return ForecastResult::Err("Error: HTTP GET request failed, error: " +
                                   String(http.errorToString(http_code).c_str()));
    }
    if (WiFi.status() != WL_CONNECTED) {
        return ForecastResult::Err("Error: WiFi not connected.");
    }
}

template typename ForecastData<FORECAST_HOURS>::result_t get_forecast<FORECAST_HOURS>(int);
