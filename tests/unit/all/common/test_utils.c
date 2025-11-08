// Test suite for workspace/all/common/utils/utils.c
// Tests timing functions

#include "../../../../workspace/all/common/utils/utils.h"
#include "../../../support/unity/unity.h"
#include <stdint.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

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

	RUN_TEST(test_getMicroseconds_non_zero);
	RUN_TEST(test_getMicroseconds_monotonic);

	return UNITY_END();
}
