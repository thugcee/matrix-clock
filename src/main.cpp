#include "secrets.h"
#include "time_utils.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <net_utils.h>
#include <stdio.h>
#include <sys/time.h> // At the top of your file
#include <time.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0,
                     60 * 60 * 1000); // UTC offset (0), update interval 1h

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
    setupNTP(timeClient);

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
