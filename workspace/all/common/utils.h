/**
 * utils.h - Core utility function declarations for MinUI
 *
 * Provides cross-platform utilities for string manipulation, file I/O,
 * timing, name processing, date/time, and math operations. All functions
 * are implemented in utils.c and are platform-independent.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

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
uint64_t getMicroseconds(void);

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
int prefixMatch(char* pre, char* str);

/**
 * Checks if a string ends with a given suffix (case-insensitive).
 *
 * Commonly used to check file extensions (e.g., ".pak", ".gb").
 *
 * @param suf Suffix to search for
 * @param str String to search in
 * @return 1 if str ends with suf, 0 otherwise
 */
int suffixMatch(char* suf, char* str);

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
int exactMatch(const char* str1, const char* str2);

/**
 * Checks if haystack contains needle (case-insensitive).
 *
 * @param haystack String to search in
 * @param needle Substring to search for
 * @return 1 if needle is found in haystack, 0 otherwise
 */
int containsString(char* haystack, char* needle);

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
int hide(char* file_name);

/**
 * Converts Windows line endings (CRLF) to Unix line endings (LF).
 *
 * Modifies the string in place. Useful when reading text files that
 * may have been edited on Windows.
 *
 * @param line String to normalize (modified in place)
 */
void normalizeNewline(char* line);

/**
 * Removes all trailing newline characters from a string.
 *
 * Handles multiple consecutive newlines. Modifies string in place.
 *
 * @param line String to trim (modified in place)
 */
void trimTrailingNewlines(char* line);

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
void trimSortingMeta(char** str);

/**
 * Maximum number of lines supported by splitTextLines().
 */
#define MAX_TEXT_LINES 16

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
int splitTextLines(char* str, char** lines, int max_lines);

///////////////////////////////
// File I/O utilities
///////////////////////////////

/**
 * Checks if a file or directory exists.
 *
 * @param path Path to check
 * @return 1 if path exists, 0 otherwise
 */
int exists(char* path);

/**
 * Creates an empty file or updates its timestamp.
 *
 * Equivalent to the Unix 'touch' command.
 *
 * @param path Path to file
 */
void touch(char* path);

/**
 * Writes a string to a file, overwriting existing content.
 *
 * Creates the file if it doesn't exist. Silently fails if
 * the file cannot be opened (e.g., permissions, disk full).
 *
 * @param path Path to file
 * @param contents String to write (null-terminated)
 */
void putFile(const char* path, const char* contents);

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
void getFile(const char* path, char* buffer, size_t buffer_size);

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
char* allocFile(const char* path);

/**
 * Writes an integer to a text file as a decimal string.
 *
 * Overwrites existing file content.
 *
 * @param path Path to file
 * @param value Integer to write
 */
void putInt(const char* path, int value);

/**
 * Reads an integer from a text file.
 *
 * Useful for reading simple config values. Supports decimal,
 * octal (0-prefix), and hex (0x-prefix) formats.
 *
 * @param path Path to file
 * @return Integer value, or 0 if file doesn't exist or is invalid
 */
int getInt(const char* path);

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
void getDisplayName(const char* in_name, char* out_name);

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
void getEmuName(const char* in_name, char* out_name);

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
void getEmuPath(char* emu_name, char* pak_path);

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
int isLeapYear(uint32_t year);

/**
 * Gets the number of days in a given month.
 *
 * Handles leap years for February.
 *
 * @param month Month number (1=January, 12=December)
 * @param year Year (needed for February leap year calculation)
 * @return Number of days in the month (28-31)
 */
int getDaysInMonth(int month, uint32_t year);

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
                      int32_t* second);

/**
 * Converts 24-hour time to 12-hour format.
 *
 * @param hour24 Hour in 24-hour format (0-23)
 * @return Hour in 12-hour format (1-12)
 *
 * @note Does not return AM/PM indicator
 */
int convertTo12Hour(int hour24);

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
int gcd(int a, int b);

/**
 * Averages two RGB565 pixel values.
 *
 * Used for bilinear filtering during video scaling. On 32-bit ARM
 * platforms, uses optimized assembly. Otherwise uses portable C.
 *
 * RGB565 format: RRRRRGGGGGGBBBBB
 *
 * @param c1 First RGB565 pixel value
 * @param c2 Second RGB565 pixel value
 * @return Average RGB565 value
 */
uint32_t average16(uint32_t c1, uint32_t c2);

/**
 * Averages two pairs of RGB565 pixels (32-bit packed).
 *
 * Processes 2 RGB565 pixels at once for 2x throughput. On 32-bit ARM
 * platforms, uses optimized assembly. Otherwise uses portable C.
 *
 * @param c1 First pair of RGB565 pixels (packed)
 * @param c2 Second pair of RGB565 pixels (packed)
 * @return Average of both pixel pairs
 */
uint32_t average32(uint32_t c1, uint32_t c2);

#endif
