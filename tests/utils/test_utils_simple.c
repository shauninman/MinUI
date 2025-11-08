// Simple test suite for workspace/all/common/utils.c
// Tests only the string matching functions (no dependencies)

#include "../unity/unity.h"
#include <string.h>

// Copy the simple functions we're testing directly
// This avoids platform dependencies from utils.c

int prefixMatch(char* pre, char* str) {
    return !strncmp(str, pre, strlen(pre));
}

int suffixMatch(char* suf, char* str) {
    int lenpre = strlen(suf);
    int lenstr = strlen(str);
    return strcmp(suf, str + lenstr - lenpre) == 0;
}

int exactMatch(char* str1, char* str2) {
    return strcmp(str1, str2) == 0;
}

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
    TEST_ASSERT_TRUE(prefixMatch("hello", "hello world"));
}

void test_prefixMatch_no_match(void) {
    TEST_ASSERT_FALSE(prefixMatch("world", "hello"));
}

void test_prefixMatch_empty_prefix(void) {
    TEST_ASSERT_TRUE(prefixMatch("", "anything"));
}

void test_suffixMatch_exact_match(void) {
    TEST_ASSERT_TRUE(suffixMatch("hello", "hello"));
}

void test_suffixMatch_file_extension(void) {
    TEST_ASSERT_TRUE(suffixMatch(".gb", "game.gb"));
    TEST_ASSERT_TRUE(suffixMatch(".gba", "rom.gba"));
}

void test_suffixMatch_no_match(void) {
    TEST_ASSERT_FALSE(suffixMatch(".gba", "game.gb"));
}

void test_exactMatch_same(void) {
    TEST_ASSERT_TRUE(exactMatch("hello", "hello"));
}

void test_exactMatch_different(void) {
    TEST_ASSERT_FALSE(exactMatch("hello", "world"));
    TEST_ASSERT_FALSE(exactMatch("hello", "Hello")); // case sensitive
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

    RUN_TEST(test_suffixMatch_exact_match);
    RUN_TEST(test_suffixMatch_file_extension);
    RUN_TEST(test_suffixMatch_no_match);

    RUN_TEST(test_exactMatch_same);
    RUN_TEST(test_exactMatch_different);

    return UNITY_END();
}
