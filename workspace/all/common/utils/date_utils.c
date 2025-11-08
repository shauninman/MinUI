// Date/time utility functions
#include "date_utils.h"

int isLeapYear(uint32_t year) {
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int getDaysInMonth(int month, uint32_t year) {
	switch (month) {
	case 2:
		return isLeapYear(year) ? 29 : 28;
	case 4:
	case 6:
	case 9:
	case 11:
		return 30;
	default:
		return 31;
	}
}

void validateDateTime(int32_t* year, int32_t* month, int32_t* day, int32_t* hour, int32_t* minute,
                      int32_t* second) {
	// Month wrapping
	if (*month > 12)
		*month -= 12;
	else if (*month < 1)
		*month += 12;

	// Year clamping
	if (*year > 2100)
		*year = 2100;
	else if (*year < 1970)
		*year = 1970;

	// Day validation (depends on month and leap year)
	int max_days = getDaysInMonth(*month, *year);
	if (*day > max_days)
		*day -= max_days;
	else if (*day < 1)
		*day += max_days;

	// Time wrapping
	if (*hour > 23)
		*hour -= 24;
	else if (*hour < 0)
		*hour += 24;

	if (*minute > 59)
		*minute -= 60;
	else if (*minute < 0)
		*minute += 60;

	if (*second > 59)
		*second -= 60;
	else if (*second < 0)
		*second += 60;
}

int convertTo12Hour(int hour24) {
	if (hour24 == 0)
		return 12;
	else if (hour24 > 12)
		return hour24 - 12;
	else
		return hour24;
}
