// Test suite for workspace/all/common/utils/file_utils.c
// Tests file I/O functions

#include "../../../../workspace/all/common/utils/file_utils.h"
#include "../../../support/unity/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Unity requires these
void setUp(void) {}
void tearDown(void) {}

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
// Main Test Runner
// ============================================================================

int main(void) {
	UNITY_BEGIN();

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

	return UNITY_END();
}
