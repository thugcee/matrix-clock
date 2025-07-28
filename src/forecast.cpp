#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WString.h>
#include <result.hpp>

StringResult getForecast() {
    if (WiFi.status() != WL_CONNECTED) {
        return StringResult::Err("Error: WiFi not connected.");
    }

    HTTPClient http;
    String apiURL =
        "https://api.open-meteo.com/v1/"
        "forecast?latitude=53.4289&longitude=14.553&hourly=temperature_2m&forecast_days=1";

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
                return StringResult::Err("Error: JSON deserialization failed: " + String(error.c_str()));
            }

            // Check if the expected data is present
            if (!doc.containsKey("hourly") || !doc["hourly"].containsKey("temperature_2m")) {
                http.end();
                return StringResult::Err("Error: Invalid JSON format from API.");
            }

            JsonArray temps = doc["hourly"]["temperature_2m"].as<JsonArray>();

            if (temps.isNull() || temps.size() < 10) {
                http.end();
                return StringResult::Err("Error: Not enough forecast data available.");
            }

            // Find min and max temperature in the next 10 hours
            float minTemp = temps[0].as<float>();
            float maxTemp = temps[0].as<float>();

            for (int i = 1; i < 10; i++) {
                float currentTemp = temps[i].as<float>();
                if (currentTemp < minTemp) {
                    minTemp = currentTemp;
                }
                if (currentTemp > maxTemp) {
                    maxTemp = currentTemp;
                }
            }

            http.end(); // Free resources
            return StringResult::Ok("Min: " + String(minTemp, 1) + "°C, Max: " + String(maxTemp, 1) + "°C");

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
