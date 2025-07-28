#include <WString.h>
#include <stdio.h>
#include <sys/time.h>

String getFormattedLocalTime(const char* format) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), format, &timeinfo);
    return String(buf);
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
