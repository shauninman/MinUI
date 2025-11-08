# Testing Setup

MinUI uses the Unity test framework for unit testing.

## Running Tests

```bash
make -f Makefile.qa test
```

Output:
```
Building utils tests...
✓ Tests compiled
Running tests...
...
9 Tests 0 Failures 0 Ignored
OK
✓ All tests passed
```

## What's Tested

### String Utilities (9 tests)

Tests for `prefixMatch()`, `suffixMatch()`, and `exactMatch()` functions:

- Exact string matching
- Prefix detection (e.g., "hello" matches "hello world")
- Suffix detection (e.g., ".gb" matches "game.gb")
- Edge cases (empty strings, no matches)
- Case sensitivity

## Project Structure

```
tests/
├── unity/                    # Unity test framework
│   ├── unity.c
│   ├── unity.h
│   └── unity_internals.h
└── utils/                    # Utils tests
    └── test_utils_simple.c  # String function tests
```

## Adding New Tests

### 1. Create Test File

```bash
touch tests/mynewtest/test_mynewtest.c
```

### 2. Write Tests

```c
#include "../unity/unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_something(void) {
    TEST_ASSERT_EQUAL(expected, actual);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}
```

### 3. Add to Makefile.qa

```makefile
tests/mynewtest_test: tests/mynewtest/test_mynewtest.c tests/unity/unity.c
	@$(CC) -o $@ $^ -I tests -std=c99

test: tests/mynewtest_test
	@./tests/mynewtest_test
```

## Unity Assertions

Common assertions:

```c
// Basic comparisons
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_NOT_EQUAL(expected, actual)

// Strings
TEST_ASSERT_EQUAL_STRING("expected", actual)
TEST_ASSERT_EQUAL_STRING_LEN("exp", actual, 3)

// Numbers
TEST_ASSERT_EQUAL_INT(42, value)
TEST_ASSERT_EQUAL_UINT(42, value)

// Pointers
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)

// Arrays/Memory
TEST_ASSERT_EQUAL_MEMORY(expected, actual, length)
```

## Current Limitations

### Platform Dependencies

Many MinUI functions depend on platform-specific code:
- `api.c` requires SDL and platform headers
- `minui.c` requires platform definitions
- `minarch.c` requires libretro

**Solution:** Test simple, pure functions first (like string utilities).

### Future Tests

As code is refactored, we'll add tests for:

1. **Phase 1:** More util functions (file operations with mocks)
2. **Phase 2:** Config parsing
3. **Phase 3:** Save state logic
4. **Phase 4:** Input mapping
5. **Phase 5:** Integration tests

## Benefits

### Current
- ✅ Proves testing works
- ✅ Documents expected behavior
- ✅ Catches regressions in string utilities
- ✅ Template for future tests

### Future
- Enables safe refactoring
- Catches bugs before hardware testing
- Faster development iteration
- Confidence in changes

## No CI Yet

Currently tests run locally only. Future: Add to GitHub Actions.

## Clean Up

```bash
make -f Makefile.qa clean-tests
```

## Next Steps

1. ✅ **Tests work** - You are here
2. ⏭️ **Fix static analysis issue** - api.c:355 undefined behavior
3. ⏭️ **Add more tests** - As code is refactored
4. ⏭️ **Add CI** - When ready for automation
