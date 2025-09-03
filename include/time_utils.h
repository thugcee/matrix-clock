#pragma once
#include <WString.h>

/**
 * @file time_utils.h
 * @brief Utility functions for time management and formatting.
 *
 * All functions use only POSIX time functions (time, localtime_r, gmtime_r,
 * strftime) for all operations.
 */

/**
 * @brief Returns the current time as epoch seconds.
 * 
 * This function retrieves the current time from the system and returns it
 * as the number of seconds since the Unix epoch (January 1, 1970).
 * @return int64_t The current time in epoch seconds.
 */
int64_t get_current_epoch_second();

/**
 * @brief Returns the system uptime in seconds.
 *
 * This function retrieves the system uptime since boot and returns it
 * as the number of seconds.
 * @return unsigned long The system uptime in seconds.
 */
unsigned long get_uptime_millis();

 /**
 * @brief Waits until the start of the next minute.
 *
 * This function calculates the time remaining until the next full minute
 * and puts the calling task to sleep for that duration using FreeRTOS's
 * vTaskDelay function.
 */
void wait_until_next_minute(void);

/**
 * @brief Returns the current second within the current minute.
 *
 * This function retrieves the second value (0-59) of the current minute
 * from the system clock.
 *
 * @return int The current second (0-59) in the current minute.
 */
int get_current_second_in_minute();

/**
 * @brief Returns the current hour in GMT timezone.
 *
 * This function retrieves the current hour (0-23) in GMT from the system clock.
 *
 * @return int The current hour in GMT (0-23).
 */
int get_GMT_hour();

/**
 * @brief Retrieves the current local time as a struct tm.
 *
 * This function gets the current system time and converts it to local time
 * using the local timezone settings.
 * @return struct tm representing the current local time.
 */
struct tm get_local_time();

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
String get_formatted_local_time(const char* format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Formats a duration given in milliseconds into a human-readable string.
 *
 * Converts the input milliseconds into a string formatted as "HH:MM:SS.mmm",
 * where HH is hours, MM is minutes, SS is seconds, and mmm is milliseconds.
 *
 * @param rawMillis The duration in milliseconds to format.
 * @return A String representing the formatted time.
 */
String format_millis(unsigned long rawMillis);
