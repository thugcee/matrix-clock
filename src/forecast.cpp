#include "config.h"
#include "result.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WString.h>

StringResult getForecast(int startHour) {
    if (WiFi.status() != WL_CONNECTED) {
        return StringResult::Err("Error: WiFi not connected.");
    }

    HTTPClient http;
    String apiURL =
        "https://api.open-meteo.com/v1/"
        "forecast?latitude=53.4289&longitude=14.553&hourly=temperature_2m&forecast_days=2";

    // Start the HTTP request
    http.begin(apiURL);
    int httpCode = http.GET();

    // --- HTTP Response Handling ---
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            // --- JSON Parsing and Data Processing (for ArduinoJson v7.x) ---
            JsonDocument doc; // Use JsonDocument for v7. It handles its own memory.
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                http.end();
                // Return a more specific JSON error
                return StringResult::Err("Error: JSON deserialization failed: " +
                                         String(error.c_str()));
            }

            // Check if the expected data is present
            if (!doc["hourly"].is<JsonObject>() ||
                !doc["hourly"]["temperature_2m"].is<JsonArray>()) {
                http.end();
                return StringResult::Err("Error: Invalid JSON format from API.");
            }

            JsonArray temps = doc["hourly"]["temperature_2m"].as<JsonArray>();

            if (temps.isNull() || temps.size() <= startHour + FORECAST_HOURS) {
                http.end();
                return StringResult::Err("Error: Not enough forecast data available.");
            }

            // Find min and max temperature in the next FORECAST_HOURS hours
            float minTemp = temps[0].as<float>();
            float maxTemp = temps[0].as<float>();

            for (int i = startHour; i < temps.size() && i < startHour + FORECAST_HOURS; i++) {
                float currentTemp = temps[i].as<float>();
                Serial.printf("Hour %02d: Temp = %.2f\n", i >= 24 ? i - 24 : i, currentTemp);
                if (currentTemp < minTemp) {
                    minTemp = currentTemp;
                }
                if (currentTemp > maxTemp) {
                    maxTemp = currentTemp;
                }
            }

            http.end(); // Free resources
            return StringResult::Ok(String((int)round(minTemp)) + "-" + String((int)round(maxTemp)) + "°");

        } else {
            http.end();
            // Provide a more descriptive HTTP error message
            return StringResult::Err("Error: HTTP request failed, code: " + String(httpCode));
        }
    } else {
        http.end();
        // Provide a more descriptive client-side error
        return StringResult::Err("Error: HTTP GET request failed, error: " +
                                 String(http.errorToString(httpCode).c_str()));
    }
    if (WiFi.status() != WL_CONNECTED) {
        return StringResult::Err("Error: WiFi not connected.");
    }
}
