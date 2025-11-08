// Test suite for workspace/all/common/utils.c

#include "../../workspace/all/common/utils.h"
#include "../unity/unity.h"
#include <string.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// String Matching Tests
// ============================================================================

void test_prefixMatch_exact_match(void) {
	TEST_ASSERT_TRUE(prefixMatch("hello", "hello"));
}

void test_prefixMatch_longer_string(void) {
	TEST_ASSERT_TRUE(prefixMatch("hello world", "hello"));
}

void test_prefixMatch_no_match(void) {
	TEST_ASSERT_FALSE(prefixMatch("hello", "world"));
}

void test_prefixMatch_empty_prefix(void) {
	TEST_ASSERT_TRUE(prefixMatch("anything", ""));
}

void test_prefixMatch_empty_string(void) {
	TEST_ASSERT_FALSE(prefixMatch("", "hello"));
}

void test_suffixMatch_exact_match(void) {
	TEST_ASSERT_TRUE(suffixMatch("hello", "hello"));
}

void test_suffixMatch_file_extension(void) {
	TEST_ASSERT_TRUE(suffixMatch("game.gb", ".gb"));
	TEST_ASSERT_TRUE(suffixMatch("rom.gba", ".gba"));
}

void test_suffixMatch_no_match(void) {
	TEST_ASSERT_FALSE(suffixMatch("game.gb", ".gba"));
}

void test_suffixMatch_empty_suffix(void) {
	TEST_ASSERT_TRUE(suffixMatch("anything", ""));
}

void test_exactMatch_same(void) {
	TEST_ASSERT_TRUE(exactMatch("hello", "hello"));
}

void test_exactMatch_different(void) {
	TEST_ASSERT_FALSE(exactMatch("hello", "world"));
	TEST_ASSERT_FALSE(exactMatch("hello", "Hello")); // case sensitive
}

// ============================================================================
// Path/Filename Tests
// ============================================================================

void test_getFilename_with_path(void) {
	const char* result = getFilename("/path/to/file.txt");
	TEST_ASSERT_EQUAL_STRING("file.txt", result);
}

void test_getFilename_no_path(void) {
	const char* result = getFilename("file.txt");
	TEST_ASSERT_EQUAL_STRING("file.txt", result);
}

void test_getFilename_trailing_slash(void) {
	const char* result = getFilename("/path/to/");
	TEST_ASSERT_EQUAL_STRING("", result);
}

void test_getExtension_normal(void) {
	const char* result = getExtension("file.txt");
	TEST_ASSERT_EQUAL_STRING(".txt", result);
}

void test_getExtension_multiple_dots(void) {
	const char* result = getExtension("archive.tar.gz");
	TEST_ASSERT_EQUAL_STRING(".gz", result);
}

void test_getExtension_no_extension(void) {
	const char* result = getExtension("filename");
	TEST_ASSERT_EQUAL_STRING("", result);
}

void test_getExtension_hidden_file(void) {
	const char* result = getExtension(".gitignore");
	TEST_ASSERT_EQUAL_STRING("", result);
}

void test_trimExtension_normal(void) {
	char filename[256] = "game.gb";
	trimExtension(filename);
	TEST_ASSERT_EQUAL_STRING("game", filename);
}

void test_trimExtension_no_extension(void) {
	char filename[256] = "filename";
	trimExtension(filename);
	TEST_ASSERT_EQUAL_STRING("filename", filename);
}

void test_trimExtension_multiple_dots(void) {
	char filename[256] = "archive.tar.gz";
	trimExtension(filename);
	TEST_ASSERT_EQUAL_STRING("archive.tar", filename);
}

// ============================================================================
// Display Name Tests
// ============================================================================

void test_getDisplayName_removes_extension(void) {
	char result[256];
	getDisplayName("Super Mario Bros.nes", result, 256);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", result);
}

void test_getDisplayName_removes_region_tags(void) {
	char result[256];
	getDisplayName("Game (USA).gb", result, 256);
	TEST_ASSERT_EQUAL_STRING("Game", result);
}

void test_getDisplayName_removes_version(void) {
	char result[256];
	getDisplayName("Game (v1.1).gba", result, 256);
	TEST_ASSERT_EQUAL_STRING("Game", result);
}

void test_getDisplayName_complex_name(void) {
	char result[256];
	getDisplayName("Super Mario Bros (USA) (Rev 1).nes", result, 256);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", result);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

	// String matching
	RUN_TEST(test_prefixMatch_exact_match);
	RUN_TEST(test_prefixMatch_longer_string);
	RUN_TEST(test_prefixMatch_no_match);
	RUN_TEST(test_prefixMatch_empty_prefix);
	RUN_TEST(test_prefixMatch_empty_string);

	RUN_TEST(test_suffixMatch_exact_match);
	RUN_TEST(test_suffixMatch_file_extension);
	RUN_TEST(test_suffixMatch_no_match);
	RUN_TEST(test_suffixMatch_empty_suffix);

	RUN_TEST(test_exactMatch_same);
	RUN_TEST(test_exactMatch_different);

	// Path/filename
	RUN_TEST(test_getFilename_with_path);
	RUN_TEST(test_getFilename_no_path);
	RUN_TEST(test_getFilename_trailing_slash);

	RUN_TEST(test_getExtension_normal);
	RUN_TEST(test_getExtension_multiple_dots);
	RUN_TEST(test_getExtension_no_extension);
	RUN_TEST(test_getExtension_hidden_file);

	RUN_TEST(test_trimExtension_normal);
	RUN_TEST(test_trimExtension_no_extension);
	RUN_TEST(test_trimExtension_multiple_dots);

	// Display names
	RUN_TEST(test_getDisplayName_removes_extension);
	RUN_TEST(test_getDisplayName_removes_region_tags);
	RUN_TEST(test_getDisplayName_removes_version);
	RUN_TEST(test_getDisplayName_complex_name);

	return UNITY_END();
}
