#include "secrets.h"
#include "time_utils.h"
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <forecast.h>
#include <net_utils.h>
#include <stdio.h>
#include <sys/time.h> // At the top of your file
#include <time.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0,
                     60 * 60 * 1000); // UTC offset (0), update interval 1h

MD_Parola P = MD_Parola(DISPLAY_HARDWARE_TYPE, DISPLAY_DATA_PIN, DISPLAY_CLK_PIN, DISPLAY_CS_PIN,
                        DISPLAY_MAX_DEVICES);

bool forecastEnabled = true; // Flag to indicate if forecast is enabled
String forecastData = "Loading forecast...";
SemaphoreHandle_t forecastMutex; // Mutex for protecting access to forecastData

/**
 * @brief FreeRTOS task that runs periodically to update the weather forecast.
 * @param pvParameters Task parameters (not used here).
 */
void weatherUpdateTask(void* pvParameters) {
    Serial.println("Weather update task started.");
    for (;;) { // Infinite loop for the task
        Serial.println("[Task] Fetching new weather forecast...");
        StringResult newForecast = getForecast();
        String newForecastStr;
        if (newForecast.isErr()) {
            Serial.println("[Task] Error fetching forecast: " + newForecast.unwrapErr());
            newForecastStr = "Error";
        } else {
            Serial.println("[Task] Successfully fetched forecast.");
            newForecastStr = newForecast.unwrap();
        }
        
        if (xSemaphoreTake(forecastMutex, portMAX_DELAY) == pdTRUE) {
            forecastData = newForecastStr;
            xSemaphoreGive(forecastMutex);
            Serial.println("[Task] Forecast updated successfully.");
        }

        // Wait for one hour before the next run
        const int updateIntervalMs = 3600 * 1000; // 1 hour
        vTaskDelay(updateIntervalMs / portTICK_PERIOD_MS);
    }
}

// Task that prints device uptime and local time periodically.
void printStatusTask(void* parameter) {
    while (true) {
        unsigned long currentMillis = millis();
        String uptime = formatMillis(currentMillis);
        String localTime = getFormattedLocalTime();
        Serial.println("⏱️  Device Uptime: " + uptime + " | Real time: " + localTime +
                       " | Forecast: " + forecastData);
        vTaskDelay(pdMS_TO_TICKS(STATUS_UPDATE_INTERVAL_SECONDS * 1000));
    }
}

bool wifi_enabled = false;
void setup() {
    Serial.begin(115200);

    forecastMutex = xSemaphoreCreateMutex();
    if (forecastMutex == NULL) {
        Serial.println("Error: Failed to create mutex.");
        forecastData = "Er:Mutex";
        forecastEnabled = false; // Disable forecast updates
    }

    wifi_enabled = setupWiFi();
    setupNTP(timeClient);

    xTaskCreatePinnedToCore(&printStatusTask, "Print Status", 8192, NULL, 1, NULL, 1);

    if (wifi_enabled && forecastEnabled) {
        xTaskCreatePinnedToCore(weatherUpdateTask, "WeatherUpdateTask", 8192, NULL, 1, NULL, 0);
    } else {
        Serial.println("Weather updates are disabled.");
    }

    // Power saving
    btStop(); // disables Bluetooth
    setCpuFrequencyMhz(80);

    P.begin();
    P.displayClear();
    P.setIntensity(DISPLAY_BRIGHTNESS);
    P.setTextAlignment(PA_CENTER);
}

void loop() {
    if (wifi_enabled) {
        timeClient.update(); // Non-blocking sync
    }

    P.printf("%s", getFormattedLocalTime("%H : %M").c_str());
    vTaskDelay(1000); // Main loop does nothing, just keeps the task running
}
