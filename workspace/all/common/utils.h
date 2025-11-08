#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

///////////////////////////////
// Timing utilities
///////////////////////////////

uint64_t getMicroseconds(void);

///////////////////////////////
// String utilities
///////////////////////////////

// String matching functions
int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(const char* str1, const char* str2);
int containsString(char* haystack, char* needle);
int hide(char* file_name);

// String manipulation functions
void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);

// Text line splitting
// Splits a string by '\n' into an array of line pointers
// Returns the number of lines found (max MAX_TEXT_LINES)
#define MAX_TEXT_LINES 16
int splitTextLines(char* str, char** lines, int max_lines);

///////////////////////////////
// File I/O utilities
///////////////////////////////

// Check if a file exists
int exists(char* path);

// Create an empty file (touch)
void touch(char* path);

// Write string contents to a file
void putFile(const char* path, const char* contents);

// Read file contents into a buffer
void getFile(const char* path, char* buffer, size_t buffer_size);

// Allocate and read file contents (caller must free)
char* allocFile(const char* path);

// Write an integer to a file
void putInt(const char* path, int value);

// Read an integer from a file
int getInt(const char* path);

///////////////////////////////
// Name processing utilities
///////////////////////////////

// Get display name from a file path
// Strips path, extension(s), region codes, and trailing whitespace
// Example: "/path/Game (USA).gb" -> "Game"
void getDisplayName(const char* in_name, char* out_name);

// Extract emulator name from ROM path
// Example: "/Roms/GB/game.gb" -> "GB" or "test (GB).gb" -> "GB"
void getEmuName(const char* in_name, char* out_name);

// Get the full path to an emulator's launch script
void getEmuPath(char* emu_name, char* pak_path);

///////////////////////////////
// Date/time utilities
///////////////////////////////

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

///////////////////////////////
// Math utilities
///////////////////////////////

// Greatest common divisor using Euclidean algorithm
int gcd(int a, int b);

// Color averaging for 16-bit RGB565 pixels
uint32_t average16(uint32_t c1, uint32_t c2);

// Color averaging for 32-bit RGBA8888 pixels
uint32_t average32(uint32_t c1, uint32_t c2);

#endif
