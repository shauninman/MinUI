// Test suite for workspace/all/common/utils/name_utils.c
// Tests display name and emulator name processing

#include "../../../../workspace/all/common/utils/name_utils.h"
#include "../../../support/unity/unity.h"
#include "../../../support/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

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
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

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

	return UNITY_END();
}
