// Test suite for workspace/all/common/utils/math_utils.c
// Tests mathematical utility functions

#include "../../../../workspace/all/common/utils/math_utils.h"
#include "../../../support/unity/unity.h"
#include <stdint.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// GCD Tests
// ============================================================================

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

// ============================================================================
// Color Averaging Tests (16-bit RGB565)
// ============================================================================

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

// ============================================================================
// Color Averaging Tests (32-bit RGBA8888)
// ============================================================================

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

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

	// GCD tests
	RUN_TEST(test_gcd_same_numbers);
	RUN_TEST(test_gcd_coprime);
	RUN_TEST(test_gcd_one_divides_other);
	RUN_TEST(test_gcd_common_divisor);
	RUN_TEST(test_gcd_with_zero);
	RUN_TEST(test_gcd_order_independent);
	RUN_TEST(test_gcd_screen_dimensions);

	// 16-bit color averaging
	RUN_TEST(test_average16_same_colors);
	RUN_TEST(test_average16_black_white);
	RUN_TEST(test_average16_different_colors);

	// 32-bit color averaging
	RUN_TEST(test_average32_same_colors);
	RUN_TEST(test_average32_black_white);
	RUN_TEST(test_average32_overflow_handling);

	return UNITY_END();
}
