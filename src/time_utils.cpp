#include <WString.h>
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

void wait_until_next_minute(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Convert to milliseconds since epoch
    int64_t ms_now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // Compute ms into current minute
    int64_t ms_into_minute = ms_now % 60000;

    // How many ms to next minute
    int64_t ms_to_next_minute = 60000 - ms_into_minute;

    // Convert ms to FreeRTOS ticks (round up to nearest tick)
    TickType_t ticks = pdMS_TO_TICKS(ms_to_next_minute);

    vTaskDelay(ticks);
}

unsigned long get_uptime_millis() {
    return esp_timer_get_time() / 1000UL;
}

int64_t get_current_epoch_second() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec;
}

struct tm get_local_time() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

int get_current_second_in_minute() {
    int second;
    struct tm currentTime = get_local_time();
    second = currentTime.tm_sec;
    return second;
}

String get_formatted_local_time(const char* format) {
    struct tm timeinfo = get_local_time();
    char buf[32];
    strftime(buf, sizeof(buf), format, &timeinfo);
    return String(buf);
}

int get_GMT_hour() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    return timeinfo.tm_hour;
}

String format_millis(unsigned long rawMillis) {
    unsigned long hours = rawMillis / 3600000;
    unsigned long minutes = (rawMillis % 3600000) / 60000;
    unsigned long seconds = (rawMillis % 60000) / 1000;
    unsigned long millisec = rawMillis % 1000;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds,
             millisec);

    return String(timeBuffer);
}
