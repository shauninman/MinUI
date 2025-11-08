// Comprehensive test suite for workspace/all/common/utils.c
// Tests all utility functions organized by category

#include "../../../../workspace/all/common/utils.h"
#include "../../../support/unity/unity.h"
#include "../../../support/platform.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

///////////////////////////////
// Timing Tests
///////////////////////////////

void test_getMicroseconds_non_zero(void) {
	uint64_t time = getMicroseconds();
	TEST_ASSERT_TRUE(time > 0);
}

void test_getMicroseconds_monotonic(void) {
	uint64_t time1 = getMicroseconds();
	// Small delay
	for (volatile int i = 0; i < 1000; i++)
		;
	uint64_t time2 = getMicroseconds();

	TEST_ASSERT_TRUE(time2 >= time1);
}

///////////////////////////////
// String Utilities Tests
///////////////////////////////

// String Matching
void test_prefixMatch_exact(void) {
	TEST_ASSERT_TRUE(prefixMatch("hello", "hello"));
}

void test_prefixMatch_prefix(void) {
	TEST_ASSERT_TRUE(prefixMatch("hel", "hello"));
}

void test_prefixMatch_no_match(void) {
	TEST_ASSERT_FALSE(prefixMatch("world", "hello"));
}

void test_prefixMatch_case_insensitive(void) {
	TEST_ASSERT_TRUE(prefixMatch("HeLLo", "hello"));
}

void test_prefixMatch_empty(void) {
	TEST_ASSERT_TRUE(prefixMatch("", "anything"));
}

void test_suffixMatch_exact(void) {
	TEST_ASSERT_TRUE(suffixMatch("hello", "hello"));
}

void test_suffixMatch_suffix(void) {
	TEST_ASSERT_TRUE(suffixMatch("llo", "hello"));
}

void test_suffixMatch_no_match(void) {
	TEST_ASSERT_FALSE(suffixMatch("world", "hello"));
}

void test_suffixMatch_case_insensitive(void) {
	TEST_ASSERT_TRUE(suffixMatch("LLO", "hello"));
}

void test_suffixMatch_extension(void) {
	TEST_ASSERT_TRUE(suffixMatch(".txt", "file.txt"));
	TEST_ASSERT_TRUE(suffixMatch(".disabled", "test.disabled"));
}

void test_exactMatch_same(void) {
	TEST_ASSERT_TRUE(exactMatch("hello", "hello"));
}

void test_exactMatch_different(void) {
	TEST_ASSERT_FALSE(exactMatch("hello", "world"));
}

void test_exactMatch_case_sensitive(void) {
	TEST_ASSERT_FALSE(exactMatch("hello", "Hello"));
}

void test_exactMatch_null_strings(void) {
	TEST_ASSERT_FALSE(exactMatch(NULL, "hello"));
	TEST_ASSERT_FALSE(exactMatch("hello", NULL));
	TEST_ASSERT_FALSE(exactMatch(NULL, NULL));
}

void test_containsString_found(void) {
	TEST_ASSERT_TRUE(containsString("hello world", "world"));
	TEST_ASSERT_TRUE(containsString("hello world", "hello"));
	TEST_ASSERT_TRUE(containsString("hello world", "o w"));
}

void test_containsString_not_found(void) {
	TEST_ASSERT_FALSE(containsString("hello world", "xyz"));
}

void test_containsString_case_insensitive(void) {
	TEST_ASSERT_TRUE(containsString("Hello World", "WORLD"));
	TEST_ASSERT_TRUE(containsString("Hello World", "world"));
}

void test_hide_hidden_file(void) {
	TEST_ASSERT_TRUE(hide(".hidden"));
	TEST_ASSERT_TRUE(hide(".gitignore"));
}

void test_hide_disabled_file(void) {
	TEST_ASSERT_TRUE(hide("test.disabled"));
	TEST_ASSERT_TRUE(hide("something.disabled"));
}

void test_hide_map_txt(void) {
	TEST_ASSERT_TRUE(hide("map.txt"));
}

void test_hide_normal_file(void) {
	TEST_ASSERT_FALSE(hide("normal.txt"));
	TEST_ASSERT_FALSE(hide("test.c"));
}

// String Manipulation
void test_normalizeNewline_windows(void) {
	char line[] = "test\r\n";
	normalizeNewline(line);
	TEST_ASSERT_EQUAL_STRING("test\n", line);
}

void test_normalizeNewline_unix(void) {
	char line[] = "test\n";
	normalizeNewline(line);
	TEST_ASSERT_EQUAL_STRING("test\n", line);
}

void test_normalizeNewline_no_newline(void) {
	char line[] = "test";
	normalizeNewline(line);
	TEST_ASSERT_EQUAL_STRING("test", line);
}

void test_trimTrailingNewlines_single(void) {
	char line[] = "test\n";
	trimTrailingNewlines(line);
	TEST_ASSERT_EQUAL_STRING("test", line);
}

void test_trimTrailingNewlines_multiple(void) {
	char line[] = "test\n\n\n";
	trimTrailingNewlines(line);
	TEST_ASSERT_EQUAL_STRING("test", line);
}

void test_trimTrailingNewlines_none(void) {
	char line[] = "test";
	trimTrailingNewlines(line);
	TEST_ASSERT_EQUAL_STRING("test", line);
}

void test_trimSortingMeta_with_number(void) {
	char buffer[] = "001) Game Name";
	char* str = buffer;
	trimSortingMeta(&str);
	TEST_ASSERT_EQUAL_STRING("Game Name", str);
}

void test_trimSortingMeta_no_number(void) {
	char buffer[] = "Game Name";
	char* str = buffer;
	char* original = str;
	trimSortingMeta(&str);
	TEST_ASSERT_EQUAL_STRING("Game Name", str);
	TEST_ASSERT_EQUAL_PTR(original, str);
}

void test_trimSortingMeta_with_space(void) {
	char buffer[] = "42)   Game";
	char* str = buffer;
	trimSortingMeta(&str);
	TEST_ASSERT_EQUAL_STRING("Game", str);
}

// Text Line Splitting
void test_splitTextLines_single_line(void) {
	char str[] = "Hello World";
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hello World", lines[0]);
}

void test_splitTextLines_multiple_lines(void) {
	char str[] = "Line 1\nLine 2\nLine 3";
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	TEST_ASSERT_EQUAL(3, count);
	TEST_ASSERT_EQUAL_PTR(str, lines[0]);
	TEST_ASSERT_EQUAL_STRING("Line 2\nLine 3", lines[1]);
	TEST_ASSERT_EQUAL_STRING("Line 3", lines[2]);
}

void test_splitTextLines_empty_string(void) {
	char str[] = "";
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	TEST_ASSERT_EQUAL(1, count);
}

void test_splitTextLines_null_string(void) {
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(NULL, lines, MAX_TEXT_LINES);
	TEST_ASSERT_EQUAL(0, count);
}

void test_splitTextLines_max_lines(void) {
	char str[] = "1\n2\n3\n4\n5";
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, 3);
	TEST_ASSERT_EQUAL(3, count); // Should stop at max_lines
}

///////////////////////////////
// File I/O Tests
///////////////////////////////

void test_exists_file_exists(void) {
	// Create a temporary file
	const char* path = "/tmp/test_exists.txt";
	FILE* f = fopen(path, "w");
	fclose(f);

	TEST_ASSERT_TRUE(exists((char*)path));

	// Clean up
	unlink(path);
}

void test_exists_file_not_exists(void) {
	TEST_ASSERT_FALSE(exists("/tmp/nonexistent_file_12345.txt"));
}

void test_touch_creates_file(void) {
	const char* path = "/tmp/test_touch.txt";

	// Make sure it doesn't exist
	unlink(path);

	touch((char*)path);
	TEST_ASSERT_TRUE(exists((char*)path));

	// Clean up
	unlink(path);
}

void test_putFile_and_allocFile(void) {
	const char* path = "/tmp/test_putfile.txt";
	const char* content = "Hello, World!";

	putFile((char*)path, (char*)content);

	char* read_content = allocFile((char*)path);
	TEST_ASSERT_NOT_NULL(read_content);
	TEST_ASSERT_EQUAL_STRING(content, read_content);

	free(read_content);
	unlink(path);
}

void test_getFile_reads_content(void) {
	const char* path = "/tmp/test_getfile.txt";
	const char* content = "Test Content";
	char buffer[256];

	// Write file
	putFile((char*)path, (char*)content);

	// Read file
	memset(buffer, 0, sizeof(buffer));
	getFile((char*)path, buffer, sizeof(buffer));

	TEST_ASSERT_EQUAL_STRING(content, buffer);

	unlink(path);
}

void test_getFile_buffer_size_limit(void) {
	const char* path = "/tmp/test_getfile_size.txt";
	const char* content = "1234567890";
	char buffer[6];

	putFile((char*)path, (char*)content);

	memset(buffer, 0, sizeof(buffer));
	getFile((char*)path, buffer, sizeof(buffer));

	// Should read only 5 chars (buffer_size - 1)
	TEST_ASSERT_EQUAL_STRING("12345", buffer);

	unlink(path);
}

void test_putInt_and_getInt(void) {
	const char* path = "/tmp/test_int.txt";
	int value = 42;

	putInt((char*)path, value);

	int read_value = getInt((char*)path);
	TEST_ASSERT_EQUAL_INT(value, read_value);

	unlink(path);
}

void test_getInt_nonexistent_file(void) {
	int value = getInt("/tmp/nonexistent_file_12345.txt");
	TEST_ASSERT_EQUAL_INT(0, value);
}

void test_putInt_negative(void) {
	const char* path = "/tmp/test_int_neg.txt";
	int value = -123;

	putInt((char*)path, value);

	int read_value = getInt((char*)path);
	TEST_ASSERT_EQUAL_INT(value, read_value);

	unlink(path);
}

void test_allocFile_nonexistent(void) {
	char* content = allocFile("/tmp/nonexistent_file_12345.txt");
	TEST_ASSERT_NULL(content);
}

///////////////////////////////
// Name Processing Tests
///////////////////////////////

void test_getDisplayName_simple(void) {
	char out[256];
	getDisplayName("test.txt", out);
	TEST_ASSERT_EQUAL_STRING("test", out);
}

void test_getDisplayName_with_path(void) {
	char out[256];
	getDisplayName("/path/to/file.txt", out);
	TEST_ASSERT_EQUAL_STRING("file", out);
}

void test_getDisplayName_multiple_extensions(void) {
	char out[256];
	getDisplayName("game.p8.png", out);
	TEST_ASSERT_EQUAL_STRING("game", out);
}

void test_getDisplayName_with_parens(void) {
	char out[256];
	getDisplayName("Game (USA).gb", out);
	TEST_ASSERT_EQUAL_STRING("Game", out);
}

void test_getDisplayName_with_brackets(void) {
	char out[256];
	getDisplayName("Game [v1.0].gba", out);
	TEST_ASSERT_EQUAL_STRING("Game", out);
}

void test_getDisplayName_with_trailing_space(void) {
	char out[256];
	getDisplayName("Game  ", out);
	TEST_ASSERT_EQUAL_STRING("Game", out);
}

void test_getDisplayName_complex(void) {
	char out[256];
	getDisplayName("/path/to/Super Mario Bros (USA) (Rev 1).nes", out);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", out);
}

void test_getDisplayName_doom_extension(void) {
	char out[256];
	getDisplayName("game.doom", out);
	TEST_ASSERT_EQUAL_STRING("game", out);
}

void test_getEmuName_simple(void) {
	char out[512];
	getEmuName("game.gb", out);
	TEST_ASSERT_EQUAL_STRING("game.gb", out);
}

void test_getEmuName_with_parens(void) {
	char out[512];
	getEmuName("test (GB).gb", out);
	TEST_ASSERT_EQUAL_STRING("GB", out);
}

///////////////////////////////
// Date/Time Tests
///////////////////////////////

// Leap Year Tests
void test_isLeapYear_divisible_by_4(void) {
	TEST_ASSERT_TRUE(isLeapYear(2024));
	TEST_ASSERT_TRUE(isLeapYear(2020));
	TEST_ASSERT_TRUE(isLeapYear(2004));
}

void test_isLeapYear_not_divisible_by_4(void) {
	TEST_ASSERT_FALSE(isLeapYear(2023));
	TEST_ASSERT_FALSE(isLeapYear(2021));
	TEST_ASSERT_FALSE(isLeapYear(2019));
}

void test_isLeapYear_century_not_divisible_by_400(void) {
	TEST_ASSERT_FALSE(isLeapYear(1900));
	TEST_ASSERT_FALSE(isLeapYear(2100));
}

void test_isLeapYear_century_divisible_by_400(void) {
	TEST_ASSERT_TRUE(isLeapYear(2000));
	TEST_ASSERT_TRUE(isLeapYear(2400));
}

// Days in Month Tests
void test_getDaysInMonth_31_day_months(void) {
	TEST_ASSERT_EQUAL(31, getDaysInMonth(1, 2024));  // January
	TEST_ASSERT_EQUAL(31, getDaysInMonth(3, 2024));  // March
	TEST_ASSERT_EQUAL(31, getDaysInMonth(5, 2024));  // May
	TEST_ASSERT_EQUAL(31, getDaysInMonth(7, 2024));  // July
	TEST_ASSERT_EQUAL(31, getDaysInMonth(8, 2024));  // August
	TEST_ASSERT_EQUAL(31, getDaysInMonth(10, 2024)); // October
	TEST_ASSERT_EQUAL(31, getDaysInMonth(12, 2024)); // December
}

void test_getDaysInMonth_30_day_months(void) {
	TEST_ASSERT_EQUAL(30, getDaysInMonth(4, 2024));  // April
	TEST_ASSERT_EQUAL(30, getDaysInMonth(6, 2024));  // June
	TEST_ASSERT_EQUAL(30, getDaysInMonth(9, 2024));  // September
	TEST_ASSERT_EQUAL(30, getDaysInMonth(11, 2024)); // November
}

void test_getDaysInMonth_february_leap_year(void) {
	TEST_ASSERT_EQUAL(29, getDaysInMonth(2, 2024)); // Leap year
	TEST_ASSERT_EQUAL(29, getDaysInMonth(2, 2000)); // Century divisible by 400
}

void test_getDaysInMonth_february_non_leap_year(void) {
	TEST_ASSERT_EQUAL(28, getDaysInMonth(2, 2023)); // Regular year
	TEST_ASSERT_EQUAL(28, getDaysInMonth(2, 1900)); // Century not divisible by 400
}

// validateDateTime Tests
void test_validateDateTime_month_wrap_high(void) {
	int32_t y = 2024, m = 13, d = 15, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, m); // 13 - 12 = 1
}

void test_validateDateTime_month_wrap_low(void) {
	int32_t y = 2024, m = 0, d = 15, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(12, m); // 0 + 12 = 12
}

void test_validateDateTime_year_clamp_high(void) {
	int32_t y = 2150, m = 6, d = 15, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(2100, y);
}

void test_validateDateTime_year_clamp_low(void) {
	int32_t y = 1950, m = 6, d = 15, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1970, y);
}

void test_validateDateTime_day_wrap_31_day_month(void) {
	int32_t y = 2024, m = 1, d = 32, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, d); // 32 - 31 = 1
}

void test_validateDateTime_day_wrap_30_day_month(void) {
	int32_t y = 2024, m = 4, d = 31, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, d); // 31 - 30 = 1
}

void test_validateDateTime_day_wrap_february_leap(void) {
	int32_t y = 2024, m = 2, d = 30, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, d); // 30 - 29 = 1
}

void test_validateDateTime_day_wrap_february_non_leap(void) {
	int32_t y = 2023, m = 2, d = 29, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, d); // 29 - 28 = 1
}

void test_validateDateTime_day_wrap_low(void) {
	int32_t y = 2024, m = 1, d = 0, h = 12, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(31, d); // 0 + 31 = 31
}

void test_validateDateTime_hour_wrap_high(void) {
	int32_t y = 2024, m = 6, d = 15, h = 25, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(1, h); // 25 - 24 = 1
}

void test_validateDateTime_hour_wrap_low(void) {
	int32_t y = 2024, m = 6, d = 15, h = -1, min = 30, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(23, h); // -1 + 24 = 23
}

void test_validateDateTime_minute_wrap_high(void) {
	int32_t y = 2024, m = 6, d = 15, h = 12, min = 65, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(5, min); // 65 - 60 = 5
}

void test_validateDateTime_minute_wrap_low(void) {
	int32_t y = 2024, m = 6, d = 15, h = 12, min = -5, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(55, min); // -5 + 60 = 55
}

void test_validateDateTime_second_wrap_high(void) {
	int32_t y = 2024, m = 6, d = 15, h = 12, min = 30, s = 70;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(10, s); // 70 - 60 = 10
}

void test_validateDateTime_second_wrap_low(void) {
	int32_t y = 2024, m = 6, d = 15, h = 12, min = 30, s = -10;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(50, s); // -10 + 60 = 50
}

void test_validateDateTime_all_valid(void) {
	int32_t y = 2024, m = 6, d = 15, h = 14, min = 30, s = 45;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(2024, y);
	TEST_ASSERT_EQUAL(6, m);
	TEST_ASSERT_EQUAL(15, d);
	TEST_ASSERT_EQUAL(14, h);
	TEST_ASSERT_EQUAL(30, min);
	TEST_ASSERT_EQUAL(45, s);
}

void test_validateDateTime_leap_day_valid(void) {
	int32_t y = 2024, m = 2, d = 29, h = 12, min = 0, s = 0;
	validateDateTime(&y, &m, &d, &h, &min, &s);
	TEST_ASSERT_EQUAL(29, d); // Should remain 29 (valid leap day)
}

// 12-Hour Conversion Tests
void test_convertTo12Hour_midnight(void) {
	TEST_ASSERT_EQUAL(12, convertTo12Hour(0));
}

void test_convertTo12Hour_morning(void) {
	TEST_ASSERT_EQUAL(1, convertTo12Hour(1));
	TEST_ASSERT_EQUAL(11, convertTo12Hour(11));
}

void test_convertTo12Hour_noon(void) {
	TEST_ASSERT_EQUAL(12, convertTo12Hour(12));
}

void test_convertTo12Hour_afternoon(void) {
	TEST_ASSERT_EQUAL(1, convertTo12Hour(13));
	TEST_ASSERT_EQUAL(11, convertTo12Hour(23));
}

void test_convertTo12Hour_edge_cases(void) {
	TEST_ASSERT_EQUAL(6, convertTo12Hour(6));  // AM stays same
	TEST_ASSERT_EQUAL(6, convertTo12Hour(18)); // 6 PM -> 6
}

///////////////////////////////
// Math Utilities Tests
///////////////////////////////

// GCD Tests
void test_gcd_same_numbers(void) {
	TEST_ASSERT_EQUAL(5, gcd(5, 5));
	TEST_ASSERT_EQUAL(10, gcd(10, 10));
}

void test_gcd_coprime(void) {
	TEST_ASSERT_EQUAL(1, gcd(17, 19));
	TEST_ASSERT_EQUAL(1, gcd(7, 11));
}

void test_gcd_one_divides_other(void) {
	TEST_ASSERT_EQUAL(5, gcd(15, 5));
	TEST_ASSERT_EQUAL(10, gcd(100, 10));
}

void test_gcd_common_divisor(void) {
	TEST_ASSERT_EQUAL(6, gcd(48, 18));  // 48 = 6*8, 18 = 6*3
	TEST_ASSERT_EQUAL(12, gcd(60, 48)); // 60 = 12*5, 48 = 12*4
}

void test_gcd_with_zero(void) {
	TEST_ASSERT_EQUAL(5, gcd(5, 0));
	TEST_ASSERT_EQUAL(10, gcd(0, 10));
}

void test_gcd_order_independent(void) {
	TEST_ASSERT_EQUAL(gcd(48, 18), gcd(18, 48));
	TEST_ASSERT_EQUAL(gcd(100, 25), gcd(25, 100));
}

void test_gcd_screen_dimensions(void) {
	// Common use case - scaling video dimensions
	TEST_ASSERT_EQUAL(160, gcd(640, 480)); // 640 = 160*4, 480 = 160*3 (4:3 ratio)
	TEST_ASSERT_EQUAL(160, gcd(320, 480)); // 320 = 160*2, 480 = 160*3 (2:3 ratio)
}

// Color Averaging Tests (16-bit RGB565)
void test_average16_same_colors(void) {
	uint32_t color = 0xF800; // Red
	TEST_ASSERT_EQUAL(color, average16(color, color));
}

void test_average16_black_white(void) {
	uint32_t black = 0x0000;
	uint32_t white = 0xFFFF;
	uint32_t result = average16(black, white);
	// Should be approximately middle gray
	TEST_ASSERT_TRUE(result > 0 && result < 0xFFFF);
}

void test_average16_different_colors(void) {
	uint32_t red = 0xF800;   // Pure red (RGB565)
	uint32_t blue = 0x001F;  // Pure blue (RGB565)
	uint32_t result = average16(red, blue);
	// Result should have components from both
	TEST_ASSERT_TRUE(result != red);
	TEST_ASSERT_TRUE(result != blue);
}

// Color Averaging Tests (32-bit RGBA8888)
void test_average32_same_colors(void) {
	uint32_t color = 0xFF0000FF; // Red with alpha
	TEST_ASSERT_EQUAL(color, average32(color, color));
}

void test_average32_black_white(void) {
	uint32_t black = 0x00000000;
	uint32_t white = 0xFFFFFFFF;
	uint32_t result = average32(black, white);
	// Should be approximately middle gray
	TEST_ASSERT_TRUE(result > 0 && result < 0xFFFFFFFF);
}

void test_average32_overflow_handling(void) {
	// Test with values that could overflow if not handled properly
	uint32_t c1 = 0xFFFFFFFF;
	uint32_t c2 = 0xFFFFFFFE;
	uint32_t result = average32(c1, c2);
	// Should handle overflow correctly
	TEST_ASSERT_TRUE(result > 0);
}

///////////////////////////////
// Main Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Timing
	RUN_TEST(test_getMicroseconds_non_zero);
	RUN_TEST(test_getMicroseconds_monotonic);

	// String matching
	RUN_TEST(test_prefixMatch_exact);
	RUN_TEST(test_prefixMatch_prefix);
	RUN_TEST(test_prefixMatch_no_match);
	RUN_TEST(test_prefixMatch_case_insensitive);
	RUN_TEST(test_prefixMatch_empty);
	RUN_TEST(test_suffixMatch_exact);
	RUN_TEST(test_suffixMatch_suffix);
	RUN_TEST(test_suffixMatch_no_match);
	RUN_TEST(test_suffixMatch_case_insensitive);
	RUN_TEST(test_suffixMatch_extension);
	RUN_TEST(test_exactMatch_same);
	RUN_TEST(test_exactMatch_different);
	RUN_TEST(test_exactMatch_case_sensitive);
	RUN_TEST(test_exactMatch_null_strings);
	RUN_TEST(test_containsString_found);
	RUN_TEST(test_containsString_not_found);
	RUN_TEST(test_containsString_case_insensitive);
	RUN_TEST(test_hide_hidden_file);
	RUN_TEST(test_hide_disabled_file);
	RUN_TEST(test_hide_map_txt);
	RUN_TEST(test_hide_normal_file);

	// String manipulation
	RUN_TEST(test_normalizeNewline_windows);
	RUN_TEST(test_normalizeNewline_unix);
	RUN_TEST(test_normalizeNewline_no_newline);
	RUN_TEST(test_trimTrailingNewlines_single);
	RUN_TEST(test_trimTrailingNewlines_multiple);
	RUN_TEST(test_trimTrailingNewlines_none);
	RUN_TEST(test_trimSortingMeta_with_number);
	RUN_TEST(test_trimSortingMeta_no_number);
	RUN_TEST(test_trimSortingMeta_with_space);

	// Text line splitting
	RUN_TEST(test_splitTextLines_single_line);
	RUN_TEST(test_splitTextLines_multiple_lines);
	RUN_TEST(test_splitTextLines_empty_string);
	RUN_TEST(test_splitTextLines_null_string);
	RUN_TEST(test_splitTextLines_max_lines);

	// File I/O
	RUN_TEST(test_exists_file_exists);
	RUN_TEST(test_exists_file_not_exists);
	RUN_TEST(test_touch_creates_file);
	RUN_TEST(test_putFile_and_allocFile);
	RUN_TEST(test_getFile_reads_content);
	RUN_TEST(test_getFile_buffer_size_limit);
	RUN_TEST(test_putInt_and_getInt);
	RUN_TEST(test_getInt_nonexistent_file);
	RUN_TEST(test_putInt_negative);
	RUN_TEST(test_allocFile_nonexistent);

	// Name processing
	RUN_TEST(test_getDisplayName_simple);
	RUN_TEST(test_getDisplayName_with_path);
	RUN_TEST(test_getDisplayName_multiple_extensions);
	RUN_TEST(test_getDisplayName_with_parens);
	RUN_TEST(test_getDisplayName_with_brackets);
	RUN_TEST(test_getDisplayName_with_trailing_space);
	RUN_TEST(test_getDisplayName_complex);
	RUN_TEST(test_getDisplayName_doom_extension);
	RUN_TEST(test_getEmuName_simple);
	RUN_TEST(test_getEmuName_with_parens);

	// Leap year
	RUN_TEST(test_isLeapYear_divisible_by_4);
	RUN_TEST(test_isLeapYear_not_divisible_by_4);
	RUN_TEST(test_isLeapYear_century_not_divisible_by_400);
	RUN_TEST(test_isLeapYear_century_divisible_by_400);

	// Days in month
	RUN_TEST(test_getDaysInMonth_31_day_months);
	RUN_TEST(test_getDaysInMonth_30_day_months);
	RUN_TEST(test_getDaysInMonth_february_leap_year);
	RUN_TEST(test_getDaysInMonth_february_non_leap_year);

	// validateDateTime
	RUN_TEST(test_validateDateTime_month_wrap_high);
	RUN_TEST(test_validateDateTime_month_wrap_low);
	RUN_TEST(test_validateDateTime_year_clamp_high);
	RUN_TEST(test_validateDateTime_year_clamp_low);
	RUN_TEST(test_validateDateTime_day_wrap_31_day_month);
	RUN_TEST(test_validateDateTime_day_wrap_30_day_month);
	RUN_TEST(test_validateDateTime_day_wrap_february_leap);
	RUN_TEST(test_validateDateTime_day_wrap_february_non_leap);
	RUN_TEST(test_validateDateTime_day_wrap_low);
	RUN_TEST(test_validateDateTime_hour_wrap_high);
	RUN_TEST(test_validateDateTime_hour_wrap_low);
	RUN_TEST(test_validateDateTime_minute_wrap_high);
	RUN_TEST(test_validateDateTime_minute_wrap_low);
	RUN_TEST(test_validateDateTime_second_wrap_high);
	RUN_TEST(test_validateDateTime_second_wrap_low);
	RUN_TEST(test_validateDateTime_all_valid);
	RUN_TEST(test_validateDateTime_leap_day_valid);

	// 12-hour conversion
	RUN_TEST(test_convertTo12Hour_midnight);
	RUN_TEST(test_convertTo12Hour_morning);
	RUN_TEST(test_convertTo12Hour_noon);
	RUN_TEST(test_convertTo12Hour_afternoon);
	RUN_TEST(test_convertTo12Hour_edge_cases);

	// GCD
	RUN_TEST(test_gcd_same_numbers);
	RUN_TEST(test_gcd_coprime);
	RUN_TEST(test_gcd_one_divides_other);
	RUN_TEST(test_gcd_common_divisor);
	RUN_TEST(test_gcd_with_zero);
	RUN_TEST(test_gcd_order_independent);
	RUN_TEST(test_gcd_screen_dimensions);

	// Color averaging (16-bit)
	RUN_TEST(test_average16_same_colors);
	RUN_TEST(test_average16_black_white);
	RUN_TEST(test_average16_different_colors);

	// Color averaging (32-bit)
	RUN_TEST(test_average32_same_colors);
	RUN_TEST(test_average32_black_white);
	RUN_TEST(test_average32_overflow_handling);

	return UNITY_END();
}
