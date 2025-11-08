// Test suite for workspace/all/common/utils/utils.c
// Tests string matching, string manipulation, and timing functions

#include "../../../../workspace/all/common/utils/utils.h"
#include "../../../support/unity/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

	// Timing
	RUN_TEST(test_getMicroseconds_non_zero);
	RUN_TEST(test_getMicroseconds_monotonic);

	return UNITY_END();
}
