#pragma once
#include <WString.h>

int getGMTHour();

struct tm getLocalTime();

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
String getFormattedLocalTime(const char* format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Formats a duration given in milliseconds into a human-readable string.
 *
 * Converts the input milliseconds into a string formatted as "HH:MM:SS.mmm",
 * where HH is hours, MM is minutes, SS is seconds, and mmm is milliseconds.
 *
 * @param rawMillis The duration in milliseconds to format.
 * @return A String representing the formatted time.
 */
String formatMillis(unsigned long rawMillis);
