/**
 * utils.c - Core utility functions for MinUI
 *
 * Provides cross-platform utilities for string manipulation, file I/O,
 * timing, and name processing. This file is shared across all platforms
 * and should not contain platform-specific code.
 */

#define _GNU_SOURCE // Required for strcasestr on glibc systems
#include "utils.h"
#include "defines.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

///////////////////////////////
// Timing utilities
///////////////////////////////

/**
 * Gets current time in microseconds since Unix epoch.
 *
 * Used for frame timing, performance measurement, and sleep calculations.
 *
 * @return Time in microseconds (1/1,000,000th of a second)
 */
uint64_t getMicroseconds(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}

///////////////////////////////
// String utilities
///////////////////////////////

/**
 * Checks if a string starts with a given prefix (case-insensitive).
 *
 * @param pre Prefix to search for
 * @param str String to search in
 * @return 1 if str starts with pre, 0 otherwise
 */
int prefixMatch(char* pre, char* str) {
	return (strncasecmp(pre, str, strlen(pre)) == 0);
}

/**
 * Checks if a string ends with a given suffix (case-insensitive).
 *
 * Commonly used to check file extensions (e.g., ".pak", ".gb").
 *
 * @param suf Suffix to search for
 * @param str String to search in
 * @return 1 if str ends with suf, 0 otherwise
 */
int suffixMatch(char* suf, char* str) {
	int len = strlen(suf);
	int offset = strlen(str) - len;
	return (offset >= 0 && strncasecmp(suf, str + offset, len) == 0);
}

/**
 * Checks if two strings are exactly equal (case-sensitive).
 *
 * Unlike strcmp, this returns 1 for match, 0 for non-match.
 * Safely handles NULL pointers.
 *
 * @param str1 First string to compare
 * @param str2 Second string to compare
 * @return 1 if strings are identical, 0 otherwise
 */
int exactMatch(const char* str1, const char* str2) {
	if (!str1 || !str2)
		return 0; // NULL isn't safe here
	size_t len1 = strlen(str1);
	if (len1 != strlen(str2))
		return 0;
	return (strncmp(str1, str2, len1) == 0);
}

/**
 * Checks if haystack contains needle (case-insensitive).
 *
 * @param haystack String to search in
 * @param needle Substring to search for
 * @return 1 if needle is found in haystack, 0 otherwise
 */
int containsString(char* haystack, char* needle) {
	return strcasestr(haystack, needle) != NULL;
}

/**
 * Determines if a file should be hidden in the UI.
 *
 * Files are hidden if they:
 * - Start with '.' (hidden files)
 * - End with '.disabled'
 * - Are named 'map.txt' (ROM mapping metadata)
 *
 * @param file_name Name of the file (with or without path)
 * @return 1 if file should be hidden, 0 if it should be shown
 */
int hide(char* file_name) {
	return file_name[0] == '.' || suffixMatch(".disabled", file_name) ||
	       exactMatch("map.txt", file_name);
}

/**
 * Converts Windows line endings (CRLF) to Unix line endings (LF).
 *
 * Modifies the string in place. Useful when reading text files that
 * may have been edited on Windows.
 *
 * @param line String to normalize (modified in place)
 */
void normalizeNewline(char* line) {
	int len = strlen(line);
	if (len > 1 && line[len - 1] == '\n' && line[len - 2] == '\r') {
		line[len - 2] = '\n';
		line[len - 1] = '\0';
	}
}

/**
 * Removes all trailing newline characters from a string.
 *
 * Handles multiple consecutive newlines. Modifies string in place.
 *
 * @param line String to trim (modified in place)
 */
void trimTrailingNewlines(char* line) {
	int len = strlen(line);
	while (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0';
		len -= 1;
	}
}

/**
 * Strips sorting metadata prefix from a string pointer.
 *
 * Advances the string pointer past prefixes like "001) " that are
 * used to control display order in file lists. If no valid prefix
 * is found, the pointer is not modified.
 *
 * Example: "001) Super Mario.gb" -> "Super Mario.gb"
 *
 * @param str Pointer to string pointer (modified in place)
 *
 * @note Modifies the pointer itself, not the string content
 */
void trimSortingMeta(char** str) {
	char* safe = *str;

	// Skip leading digits
	while (isdigit(**str))
		*str += 1;

	// Must have closing parenthesis
	if (*str[0] == ')') {
		*str += 1;
	} else {
		// No valid prefix found, restore original pointer
		*str = safe;
		return;
	}

	// Skip whitespace after parenthesis
	while (isblank(**str))
		*str += 1;
}

/**
 * Splits a text buffer into an array of line pointers.
 *
 * Does NOT modify the input string or allocate memory - it returns
 * pointers into the original buffer. Useful for processing multi-line
 * text files after reading with allocFile().
 *
 * @param str Input text buffer (not modified)
 * @param lines Output array of pointers to line starts
 * @param max_lines Maximum number of lines to process
 * @return Number of lines found (may be less than max_lines)
 *
 * @note Does not null-terminate lines - newlines remain in the buffer
 */
int splitTextLines(char* str, char** lines, int max_lines) {
	if (!str || max_lines < 1)
		return 0;

	int count = 0;
	char* tmp;
	lines[count++] = str;
	while ((tmp = strchr(lines[count - 1], '\n')) != NULL) {
		if (count + 1 > max_lines)
			break;
		lines[count++] = tmp + 1;
	}
	return count;
}

///////////////////////////////
// File I/O utilities
///////////////////////////////

/**
 * Checks if a file or directory exists.
 *
 * @param path Path to check
 * @return 1 if path exists, 0 otherwise
 */
int exists(char* path) {
	return access(path, F_OK) == 0;
}

/**
 * Creates an empty file or updates its timestamp.
 *
 * Equivalent to the Unix 'touch' command.
 *
 * @param path Path to file
 */
void touch(char* path) {
	close(open(path, O_RDWR | O_CREAT, 0777));
}

/**
 * Writes a string to a file, overwriting existing content.
 *
 * Creates the file if it doesn't exist. Silently fails if
 * the file cannot be opened (e.g., permissions, disk full).
 *
 * @param path Path to file
 * @param contents String to write (null-terminated)
 */
void putFile(const char* path, const char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}

/**
 * Reads entire file into a pre-allocated buffer.
 *
 * Buffer is always null-terminated. If file is larger than buffer,
 * only the first (buffer_size - 1) bytes are read.
 *
 * @param path Path to file
 * @param buffer Pre-allocated buffer to fill
 * @param buffer_size Size of buffer in bytes
 *
 * @note Buffer is not modified if file cannot be opened
 */
void getFile(const char* path, char* buffer, size_t buffer_size) {
	FILE* file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size > buffer_size - 1)
			size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}

/**
 * Reads entire file into dynamically allocated buffer.
 *
 * Allocates exactly the right amount of memory for the file.
 * Returns NULL if file cannot be opened or memory allocation fails.
 *
 * @param path Path to file
 * @return Pointer to allocated buffer, or NULL on failure
 *
 * @warning Caller must free returned pointer
 */
char* allocFile(const char* path) {
	char* contents = NULL;
	FILE* file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		contents = calloc(size + 1, sizeof(char));
		if (contents) {
			fseek(file, 0L, SEEK_SET);
			fread(contents, sizeof(char), size, file);
			contents[size] = '\0';
		}
		fclose(file);
	}
	return contents;
}

/**
 * Reads an integer from a text file.
 *
 * Useful for reading simple config values. Supports decimal,
 * octal (0-prefix), and hex (0x-prefix) formats.
 *
 * @param path Path to file
 * @return Integer value, or 0 if file doesn't exist or is invalid
 */
int getInt(const char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file != NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

/**
 * Writes an integer to a text file as a decimal string.
 *
 * Overwrites existing file content.
 *
 * @param path Path to file
 * @param value Integer to write
 */
void putInt(const char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

///////////////////////////////
// Name processing utilities
///////////////////////////////

/**
 * Cleans a ROM or app path for display in the UI.
 *
 * Performs multiple transformations:
 * 1. Extracts filename from full path
 * 2. Removes file extensions (including multi-part like .p8.png)
 * 3. Strips region codes and metadata in parentheses/brackets
 *    Example: "Super Mario (USA) (v1.2).nes" -> "Super Mario"
 * 4. Removes trailing whitespace
 * 5. Special handling: strips platform suffix from Tools paths
 *
 * @param in_name Input path (may be full path or just filename)
 * @param out_name Output buffer for cleaned name (min 256 bytes)
 *
 * @note If all content is removed, the previous valid name is restored
 */
void getDisplayName(const char* in_name, char* out_name) {
	char* tmp;
	char work_name[256];
	strcpy(work_name, in_name);
	strcpy(out_name, in_name);

	// Special case: hide platform suffix from Tools paths
	if (suffixMatch("/" PLATFORM, work_name)) {
		tmp = strrchr(work_name, '/');
		tmp[0] = '\0';
	}

	// Extract just the filename if we have a full path
	tmp = strrchr(work_name, '/');
	if (tmp)
		strcpy(out_name, tmp + 1);

	// Remove all file extensions (handles multi-part like .p8.png)
	// Only removes extensions between 1-4 characters (plus dot)
	while ((tmp = strrchr(out_name, '.')) != NULL) {
		int len = strlen(tmp);
		if (len > 2 && len <= 5) {
			tmp[0] = '\0'; // Extended to 5 for .doom files
		} else {
			break;
		}
	}

	// Remove trailing metadata in parentheses or brackets
	// Example: "Game (USA) [!]" -> "Game"
	strcpy(work_name, out_name);
	while ((tmp = strrchr(out_name, '(')) != NULL || (tmp = strrchr(out_name, '[')) != NULL) {
		if (tmp == out_name)
			break; // Don't remove if name would be empty
		tmp[0] = '\0';
		tmp = out_name;
	}

	// Safety check: restore previous name if we removed everything
	if (out_name[0] == '\0')
		strcpy(out_name, work_name);

	// Remove trailing whitespace
	tmp = out_name + strlen(out_name) - 1;
	while (tmp > out_name && isspace((unsigned char)*tmp))
		tmp--;
	tmp[1] = '\0';
}

/**
 * Extracts emulator/platform name from a ROM path.
 *
 * Examples:
 * - "/mnt/SDCARD/Roms/GB (Nintendo Game Boy)/game.gb" -> "GB"
 * - "/mnt/SDCARD/Roms/NES/mario.nes" -> "NES"
 * - "GBA (Game Boy Advance)" -> "GBA"
 *
 * Logic:
 * 1. If path starts with ROMS_PATH, extract first directory name
 * 2. If that directory name contains parentheses, extract content inside
 *
 * @param in_name ROM path or directory name
 * @param out_name Output buffer for emulator name (must be MAX_PATH length)
 *
 * @note Uses memmove() for safe overlapping buffer copies
 */
void getEmuName(const char* in_name, char* out_name) {
	char* tmp;
	strcpy(out_name, in_name);
	tmp = out_name;

	// Extract just the Roms folder name if this is a full path
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2)
			tmp2[0] = '\0';
		// Safe for overlapping buffers
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = out_name;
	}

	// Extract abbreviated name from within parentheses if present
	// Example: "GB (Game Boy)" -> "GB"
	tmp = strrchr(tmp, '(');
	if (tmp) {
		tmp += 1;
		// Safe for overlapping buffers
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = strchr(out_name, ')');
		tmp[0] = '\0';
	}
}

/**
 * Finds the launch script path for a given emulator.
 *
 * Searches in two locations:
 * 1. Platform-specific: /mnt/SDCARD/Emus/<platform>/<emu_name>.pak/launch.sh
 * 2. Shared: /mnt/SDCARD/Roms/Emus/<emu_name>.pak/launch.sh
 *
 * @param emu_name Emulator name (e.g., "GB", "NES")
 * @param pak_path Output buffer for full path to launch.sh
 *
 * @note pak_path is always modified, even if file doesn't exist
 */
void getEmuPath(char* emu_name, char* pak_path) {
	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	if (exists(pak_path))
		return;
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
}

///////////////////////////////
// Date/time utilities
///////////////////////////////

/**
 * Checks if a year is a leap year.
 *
 * A leap year occurs:
 * - Every 4 years, EXCEPT
 * - Every 100 years, EXCEPT
 * - Every 400 years
 *
 * @param year Year to check (e.g., 2024)
 * @return 1 if leap year, 0 otherwise
 */
int isLeapYear(uint32_t year) {
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

/**
 * Gets the number of days in a given month.
 *
 * Handles leap years for February.
 *
 * @param month Month number (1=January, 12=December)
 * @param year Year (needed for February leap year calculation)
 * @return Number of days in the month (28-31)
 */
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

/**
 * Validates and corrects date/time values.
 *
 * Used by the clock app when user adjusts time values up/down.
 * Wraps values that go out of range (e.g., hour 24 becomes 0,
 * month 13 becomes 1).
 *
 * Constraints:
 * - Year: clamped to 1970-2100
 * - Month: wraps at 1-12
 * - Day: wraps based on days in month
 * - Time: wraps at standard boundaries (24h, 60m, 60s)
 *
 * @param year Pointer to year value (modified in place)
 * @param month Pointer to month value (modified in place)
 * @param day Pointer to day value (modified in place)
 * @param hour Pointer to hour value (modified in place)
 * @param minute Pointer to minute value (modified in place)
 * @param second Pointer to second value (modified in place)
 *
 * @note Simple wrapping - doesn't handle multi-step overflow
 */
void validateDateTime(int32_t* year, int32_t* month, int32_t* day, int32_t* hour, int32_t* minute,
                      int32_t* second) {
	// Month wrapping
	if (*month > 12)
		*month -= 12;
	else if (*month < 1)
		*month += 12;

	// Year clamping (Unix epoch to arbitrary future limit)
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

/**
 * Converts 24-hour time to 12-hour format.
 *
 * @param hour24 Hour in 24-hour format (0-23)
 * @return Hour in 12-hour format (1-12)
 *
 * @note Does not return AM/PM indicator
 */
int convertTo12Hour(int hour24) {
	if (hour24 == 0)
		return 12; // Midnight = 12 AM
	else if (hour24 > 12)
		return hour24 - 12;
	else
		return hour24;
}

///////////////////////////////
// Math utilities
///////////////////////////////

/**
 * Calculates greatest common divisor using Euclidean algorithm.
 *
 * Used for aspect ratio calculations in video scaling.
 *
 * @param a First number
 * @param b Second number
 * @return GCD of a and b
 */
int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
/**
 * Averages two RGB565 pixel values (ARM assembly version).
 *
 * Used for bilinear filtering during video scaling. This optimized
 * assembly version is ~2x faster than C on 32-bit ARM devices.
 *
 * RGB565 format: RRRRRGGGGGGBBBBB
 * Mask 0x0821 handles the low bits of each channel to avoid overflow.
 *
 * @param c1 First RGB565 pixel value
 * @param c2 Second RGB565 pixel value
 * @return Average RGB565 value
 *
 * @note Only used on 32-bit ARM (ARMv5+), not on ARM64/AArch64
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	__asm__("eor %0, %2, %3\r\n"
	        "and %0, %0, %1\r\n"
	        "add %0, %3, %0\r\n"
	        "add %0, %0, %2\r\n"
	        "lsr %0, %0, #1\r\n"
	        : "=&r"(ret)
	        : "r"(lowbits), "r"(c1), "r"(c2)
	        :);
	return ret;
}
#else
/**
 * Averages two RGB565 pixel values (portable C version).
 *
 * Used when ARM assembly optimization is not available.
 * See assembly version for detailed explanation.
 *
 * @param c1 First RGB565 pixel value
 * @param c2 Second RGB565 pixel value
 * @return Average RGB565 value
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821)) >> 1;
}
#endif

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
/**
 * Averages two pairs of RGB565 pixels (ARM assembly version).
 *
 * Processes 32 bits = 2 RGB565 pixels at once for 2x throughput.
 * Handles overflow between pixels using carry flag.
 *
 * @param c1 First pair of RGB565 pixels (packed)
 * @param c2 Second pair of RGB565 pixels (packed)
 * @return Average of both pixel pairs
 *
 * @note Only used on 32-bit ARM (ARMv5+), not on ARM64/AArch64
 */
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	__asm__("eor %0, %3, %1\r\n" // XOR to find differing bits
	        "and %0, %0, %2\r\n" // Mask low bits
	        "adds %0, %1, %0\r\n" // Add with carry flag
	        "and %1, %1, #0\r\n" // Clear c2
	        "movcs %1, #0x80000000\r\n" // Save overflow bit
	        "adds %0, %0, %3\r\n" // Add c1
	        "rrx %0, %0\r\n" // Rotate right through carry
	        "orr %0, %0, %1\r\n" // Restore overflow
	        : "=&r"(ret), "+r"(c2)
	        : "r"(lowbits), "r"(c1)
	        : "cc");

	return ret;
}
#else
/**
 * Averages two pairs of RGB565 pixels (portable C version).
 *
 * Used when ARM assembly optimization is not available.
 * See assembly version for detailed explanation.
 *
 * @param c1 First pair of RGB565 pixels (packed)
 * @param c2 Second pair of RGB565 pixels (packed)
 * @return Average of both pixel pairs
 */
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) ? 0x80000000 : 0;

	return (ret >> 1) | of;
}
#endif
