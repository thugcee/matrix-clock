#include <stdio.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <driver/gpio.h>
// #include "sdkconfig.h"
// #include <Arduino.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Timezone.h>  // Handles DST rules automatically
#include <sys/time.h> // At the top of your file
#include <time.h>
#include "secrets.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60 * 60 * 1000); // UTC offset (0), update interval 1h

// Europe/Warsaw time zone rules (CET/CEST)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // DST: UTC+2 (March to October)
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};     // Standard: UTC+1 (October to March)
Timezone warsawTZ(CEST, CET);  // Timezone object with DST rules

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO (gpio_num_t) CONFIG_BLINK_GPIO

#ifndef LED_BUILTIN
#define LED_BUILTIN 4
#endif

String formatMillis(unsigned long rawMillis);
void setupWiFi();
void setupNTP();

// Print current local time in a nice format
void printCurrentTime() {
    time_t now = time(nullptr);           // Get current UTC time
    time_t local = warsawTZ.toLocal(now); // Convert to local time with DST

    struct tm timeinfo;
    localtime_r(&local, &timeinfo);       // Convert to struct tm

    char buf[40];
    strftime(buf, sizeof(buf), "🕒 Real time: %Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.println(buf);
}

// Task that prints uptime every 10 seconds
void printUptimeTask(void *parameter)
{
    while (true)
    {
        unsigned long currentMillis = millis();
        String uptime = formatMillis(currentMillis);
        Serial.println("⏱️  Device Uptime: " + uptime);
        printCurrentTime();
        vTaskDelay(pdMS_TO_TICKS(5000)); // Delay for 10 seconds
    }
}

void setup()
{
    Serial.begin(115200);

    setupWiFi();
    setupNTP();

    xTaskCreatePinnedToCore(&printUptimeTask, "Print running time", 8192, NULL, 1, NULL, 1);
    
    // Power saving
    btStop(); // disables Bluetooth
    setCpuFrequencyMhz(80);

    delay(500);
}

void loop()
{
    timeClient.update(); // Non-blocking sync
    vTaskDelay(1000); // Main loop does nothing, just keeps the task running
}

String formatMillis(unsigned long rawMillis)
{
    unsigned long hours = rawMillis / 3600000;
    unsigned long minutes = (rawMillis % 3600000) / 60000;
    unsigned long seconds = (rawMillis % 60000) / 1000;
    unsigned long millisec = rawMillis % 1000;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu",
             hours, minutes, seconds, millisec);

    return String(timeBuffer);
}

void setupWiFi()
{
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000); // Give some time for WiFi to connect

    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0)
    {
        delay(5000);
        Serial.println("  still waiting for WiFi...");
        timeout--;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Failed to connect to WiFi, disabling WiFi.");
        WiFi.mode(WIFI_OFF); // Disable WiFi if not connected
        return;
    }

    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
}

void setupNTP() {
  timeClient.begin();
  timeClient.forceUpdate(); // Blocking call to sync time immediately
  Serial.printf("Time synced: %s\n", timeClient.getFormattedTime().c_str());

  // Convert UTC to local time (auto-adjusts for DST)
  time_t utc = timeClient.getEpochTime();
  time_t local = warsawTZ.toLocal(utc);

  // Set ESP32 system time (RTC)
  struct timeval tv;
  tv.tv_sec = utc;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  Serial.printf("Warsaw Time: %02d:%02d:%02d\n", hour(local), minute(local), second(local));
}
