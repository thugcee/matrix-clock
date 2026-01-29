#include <WString.h>
#include <cstdio>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"


void wait_until_next_minute() {
    timeval tv{};
    gettimeofday(&tv, nullptr);

    // Convert to milliseconds since epoch
    const int64_t ms_now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // Compute ms in current minute
    const int64_t ms_into_minute = ms_now % 60000;

    // How many ms to next minute
    const int64_t ms_to_next_minute = 60000 - ms_into_minute + 1;

    // Convert ms to FreeRTOS ticks (round up to nearest tick)
    const TickType_t ticks = pdMS_TO_TICKS(ms_to_next_minute);

    vTaskDelay(ticks);
}

void wait_until_next_second() {
    timeval tv{};
    gettimeofday(&tv, nullptr);

    // Convert to milliseconds since epoch
    const int64_t ms_now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // Compute ms into current second
    const int64_t ms_into_minute = ms_now % 1000;

    // How many ms to next second
    const int64_t ms_to_next_minute = 1000 - ms_into_minute + 1;

    // Convert ms to FreeRTOS ticks (round up to nearest tick)
    const TickType_t ticks = pdMS_TO_TICKS(ms_to_next_minute);

    vTaskDelay(ticks);
}

unsigned long get_uptime_millis() {
    return esp_timer_get_time() / 1000UL;
}

int64_t get_current_epoch_second() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec;
}

tm get_local_time() {
    const time_t now = time(nullptr);
    tm timeinfo{};
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

int get_current_second_in_minute() {
    const tm currentTime = get_local_time();
    return currentTime.tm_sec;
}

String get_formatted_local_time(const char* format) {
    const tm timeinfo = get_local_time();
    char buf[32];
    strftime(buf, sizeof(buf), format, &timeinfo);
    return {buf};
}

String format_time_for_display() {
    const tm timeinfo = get_local_time();
    char buf[32];
    strftime(buf, sizeof(buf), "%H;%M", &timeinfo);
    return {buf};
}

int get_GMT_hour() {
    const time_t now = time(nullptr);
    tm timeinfo{};
    gmtime_r(&now, &timeinfo);
    return timeinfo.tm_hour;
}

String format_millis(const unsigned long rawMillis) {
    const unsigned long hours = rawMillis / 3600000;
    const unsigned long minutes = (rawMillis % 3600000) / 60000;
    const unsigned long seconds = (rawMillis % 60000) / 1000;
    const unsigned long millisec = rawMillis % 1000;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds,
             millisec);

    return {timeBuffer};
}
