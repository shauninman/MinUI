#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include <stdint.h>

// Check if a year is a leap year
int isLeapYear(uint32_t year);

// Get the number of days in a month for a given year
int getDaysInMonth(int month, uint32_t year);

// Validate and normalize a date/time
// - Wraps values that exceed their range (e.g., hour 25 -> hour 1)
// - Clamps year to valid range (1970-2100)
// - Handles leap years for February
// - Handles different month lengths (28/29/30/31 days)
void validateDateTime(int32_t* year, int32_t* month, int32_t* day, int32_t* hour, int32_t* minute,
                      int32_t* second);

// Convert 24-hour time to 12-hour format
// 0 -> 12, 1-12 -> 1-12, 13-23 -> 1-11
int convertTo12Hour(int hour24);

#endif
