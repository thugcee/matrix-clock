#include "config.h"
#include "secrets.h"
#include "time_utils.h"
#include <NTPClient.h>
#include <WiFi.h>

namespace net_utils {

/**
 * @brief Sets up the WiFi connection.
 *
 * This function attempts to connect to the specified WiFi network using the credentials
 * defined in the secrets.h file. It will retry for a maximum of 20 seconds before giving up.
 * If the connection is successful, it returns true; otherwise, it returns false.
 *
 * @return true if WiFi is connected, false otherwise.
 */
bool setup_wifi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000); // Give some time for WiFi to connect

    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(5000);
        Serial.println("  still waiting for WiFi...");
        retries--;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi, disabling WiFi.");
        WiFi.mode(WIFI_OFF); // Disable WiFi if not connected
        return false;
    }

    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    return true;
}

/**
 * @brief Sets up the NTP client for time synchronization.
 *
 * This function initializes the NTP client with the specified server and updates the local
 * system time. It also sets the timezone based on the TIMEZONE macro defined in secrets.h.
 *
 * @param time_client Reference to the NTPClient instance to be set up.
 */
void setup_NTP(NTPClient& time_client) {
    time_client.begin();
    while ( !time_client.update() ) {
        Serial.println("[NTP] still waiting for first synchronisation...");
        delay(10000);
    }
    Serial.printf("Synchronised UTC time: %s\n", time_client.getFormattedTime().c_str());

    time_t utc = time_client.getEpochTime();

    // Set ESP32 system time (RTC)
    struct timeval tv;
    tv.tv_sec = utc;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    setenv("TZ", TIMEZONE, 1);
    tzset();

    // Now localtime() will use timezone with DST
    Serial.printf("Synchronised local time: %s\n", get_formatted_local_time().c_str());
}

} // namespace net_utils