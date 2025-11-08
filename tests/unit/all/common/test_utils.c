// Comprehensive test suite for workspace/all/common/utils.c
// Tests all functions in utils.h

#include "../../../../workspace/all/common/utils.h"
#include "../../../support/unity/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// String Matching Tests
// ============================================================================

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

// ============================================================================
// Display Name Tests
// ============================================================================

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

// ============================================================================
// Emu Name Tests
// ============================================================================

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

// ============================================================================
// String Manipulation Tests
// ============================================================================

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

// ============================================================================
// File I/O Tests
// ============================================================================

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

// ============================================================================
// Timing Tests
// ============================================================================

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

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

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

	// Display names
	RUN_TEST(test_getDisplayName_simple);
	RUN_TEST(test_getDisplayName_with_path);
	RUN_TEST(test_getDisplayName_multiple_extensions);
	RUN_TEST(test_getDisplayName_with_parens);
	RUN_TEST(test_getDisplayName_with_brackets);
	RUN_TEST(test_getDisplayName_with_trailing_space);
	RUN_TEST(test_getDisplayName_complex);
	RUN_TEST(test_getDisplayName_doom_extension);

	// Emu names
	RUN_TEST(test_getEmuName_simple);
	RUN_TEST(test_getEmuName_with_parens);

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

	// Timing
	RUN_TEST(test_getMicroseconds_non_zero);
	RUN_TEST(test_getMicroseconds_monotonic);

	return UNITY_END();
}
