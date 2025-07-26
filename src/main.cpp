#include "secrets.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdio.h>
#include <sys/time.h> // At the top of your file
#include <time.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0,
                     60 * 60 * 1000); // UTC offset (0), update interval 1h

String formatMillis(unsigned long rawMillis);
String getFormattedLocalTime();
bool setupWiFi();
void setupNTP();

// Task that prints device uptime and local time periodically.
void printStatusTask(void* parameter) {
    while (true) {
        unsigned long currentMillis = millis();
        String uptime = formatMillis(currentMillis);
        String localTime = getFormattedLocalTime();
        Serial.println("⏱️  Device Uptime: " + uptime + " | Real time: " + localTime);
        vTaskDelay(pdMS_TO_TICKS(STATUS_UPDATE_INTERVAL_SECONDS * 1000));
    }
}

bool wifi_enabled = false;
void setup() {
    Serial.begin(115200);

    wifi_enabled = setupWiFi();
    setupNTP();

    xTaskCreatePinnedToCore(&printStatusTask, "Print Status", 8192, NULL, 1, NULL, 1);

    // Power saving
    btStop(); // disables Bluetooth
    setCpuFrequencyMhz(80);
}

void loop() {
    if (wifi_enabled) {
        timeClient.update(); // Non-blocking sync
    }
    vTaskDelay(1000); // Main loop does nothing, just keeps the task running
}

String formatMillis(unsigned long rawMillis) {
    unsigned long hours = rawMillis / 3600000;
    unsigned long minutes = (rawMillis % 3600000) / 60000;
    unsigned long seconds = (rawMillis % 60000) / 1000;
    unsigned long millisec = rawMillis % 1000;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds,
             millisec);

    return String(timeBuffer);
}

bool setupWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000); // Give some time for WiFi to connect

    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(5000);
        Serial.println("  still waiting for WiFi...");
        timeout--;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi, disabling WiFi.");
        WiFi.mode(WIFI_OFF); // Disable WiFi if not connected
        return false;
    }

    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    return true;
}

void setupNTP() {
    timeClient.begin();
    timeClient.forceUpdate(); // Blocking call to sync time immediately
    Serial.printf("Synchronised UTC time: %s\n", timeClient.getFormattedTime().c_str());

    time_t utc = timeClient.getEpochTime();

    // Set ESP32 system time (RTC)
    struct timeval tv;
    tv.tv_sec = utc;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    setenv("TZ", TIMEZONE, 1);
    tzset();

    // Now localtime() will use timezone with DST
    Serial.printf("Synchronised local time: %s\n", getFormattedLocalTime().c_str());
}

/**
 * @brief Returns the current local time as a formatted string.
 *
 * This function retrieves the current system time, converts it to local time,
 * and formats it as a string in the form "🕒 Real time: YYYY-MM-DD HH:MM:SS".
 * It uses only POSIX time functions (time, localtime_r, strftime) for all
 * operations.
 *
 * @return String containing the formatted local time.
 */
String getFormattedLocalTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}
