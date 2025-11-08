// Test suite for workspace/all/common/utils/date_utils.c
// Tests date/time validation logic

#include "../../../../workspace/all/common/utils/date_utils.h"
#include "../../../support/unity/unity.h"
#include <stdio.h>
#include <stdlib.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Leap Year Tests
// ============================================================================

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

// ============================================================================
// Days in Month Tests
// ============================================================================

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

// ============================================================================
// validateDateTime Tests
// ============================================================================

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

// ============================================================================
// 12-Hour Conversion Tests
// ============================================================================

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

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

	// Leap year tests
	RUN_TEST(test_isLeapYear_divisible_by_4);
	RUN_TEST(test_isLeapYear_not_divisible_by_4);
	RUN_TEST(test_isLeapYear_century_not_divisible_by_400);
	RUN_TEST(test_isLeapYear_century_divisible_by_400);

	// Days in month tests
	RUN_TEST(test_getDaysInMonth_31_day_months);
	RUN_TEST(test_getDaysInMonth_30_day_months);
	RUN_TEST(test_getDaysInMonth_february_leap_year);
	RUN_TEST(test_getDaysInMonth_february_non_leap_year);

	// validateDateTime tests
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

	return UNITY_END();
}
