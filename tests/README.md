# MinUI Test Suite

This directory contains the test suite for MinUI, organized to mirror the source code structure.

## Directory Structure

```
tests/
├── unit/                       # Unit tests (mirror workspace/ structure)
│   └── all/
│       └── common/
│           └── test_utils.c    # Tests for workspace/all/common/utils.c
├── integration/                # Integration tests (end-to-end tests)
├── fixtures/                   # Test data, sample ROMs, configs
├── support/                    # Test infrastructure
│   ├── unity/                  # Unity test framework
│   └── platform.h              # Platform stubs for testing
└── README.md                   # This file
```

## Organization Philosophy

### Mirror Structure
Tests mirror the source code structure under `workspace/`:

```
workspace/all/common/utils.c  →  tests/unit/all/common/test_utils.c
workspace/all/minui/minui.c   →  tests/unit/all/minui/test_minui.c
```

This makes it easy to:
- Find tests for any source file
- Maintain consistency as the codebase grows
- Understand test coverage at a glance

### Test Types

**Unit Tests** (`unit/`)
- Test individual functions in isolation
- Fast execution
- Mock external dependencies
- Example: Testing string manipulation in `utils.c`

**Integration Tests** (`integration/`)
- Test multiple components working together
- Test real workflows (launch a game, save state, etc.)
- May be slower to execute

**Fixtures** (`fixtures/`)
- Sample ROM files
- Test configuration files
- Expected output data

**Support** (`support/`)
- Test frameworks (Unity)
- Shared test utilities
- Platform stubs

## Running Tests

### Quick Test
```bash
make -f Makefile.qa test
```

### Specific Test Suites
```bash
# Run only unit tests
make -f Makefile.qa test

# Run with verbose output
./tests/unit_tests -v

# Run specific test
./tests/unit_tests -n test_prefixMatch_exact
```

### Clean and Rebuild
```bash
make -f Makefile.qa clean-tests
make -f Makefile.qa test
```

## Writing New Tests

### 1. Mirror the Source Structure

If adding tests for `workspace/all/minui/minui.c`:

```bash
# Create directory
mkdir -p tests/unit/all/minui

# Create test file
touch tests/unit/all/minui/test_minui.c
```

### 2. Use Unity Framework

```c
#include "../../../../workspace/all/minui/minui.h"
#include "../../../support/unity/unity.h"

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

void test_something(void) {
    TEST_ASSERT_EQUAL_INT(42, my_function());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}
```

### 3. Update Makefile.qa

Add your test to the build:

```makefile
tests/unit_tests: tests/unit/all/minui/test_minui.c ...
    @$(CC) -o $@ $^ ...
```

### 4. Common Assertions

```c
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_STRING("expected", actual)
TEST_ASSERT_NOT_NULL(pointer)
TEST_ASSERT_NULL(pointer)
```

See `support/unity/unity.h` for full list.

## Test Guidelines

### Good Test Characteristics
1. **Fast** - Unit tests should run in milliseconds
2. **Isolated** - No dependencies on other tests
3. **Repeatable** - Same result every time
4. **Self-checking** - Asserts verify correctness
5. **Timely** - Written alongside the code

### What to Test
- **Happy paths** - Normal, expected usage
- **Edge cases** - Boundary conditions (empty strings, NULL, max values)
- **Error cases** - Invalid input, file not found, etc.
- **Regression tests** - Known bugs that were fixed

### Example Test Coverage

From `test_utils.c`:
```c
// Happy path
void test_prefixMatch_exact(void) {
    TEST_ASSERT_TRUE(prefixMatch("hello", "hello"));
}

// Edge case
void test_prefixMatch_empty(void) {
    TEST_ASSERT_TRUE(prefixMatch("", "anything"));
}

// Error case
void test_exactMatch_null_strings(void) {
    TEST_ASSERT_FALSE(exactMatch(NULL, "hello"));
}

// Regression test
void test_getEmuName_with_parens(void) {
    // This previously crashed due to overlapping memory
    char out[512];
    getEmuName("test (GB).gb", out);
    TEST_ASSERT_EQUAL_STRING("GB", out);
}
```

## Current Test Coverage

### workspace/all/common/utils.c - ✅ 52 tests
**File:** `tests/unit/all/common/test_utils.c`

- String matching functions (prefixMatch, suffixMatch, exactMatch, containsString, hide)
- Display name processing (getDisplayName)
- Emulator name extraction (getEmuName)
- String manipulation (normalizeNewline, trimTrailingNewlines, trimSortingMeta)
- File I/O (exists, touch, putFile, getFile, allocFile, putInt, getInt)
- Timing (getMicroseconds)

**Coverage:** All public functions tested with happy paths, edge cases, and error conditions.

### Todo
- [ ] workspace/all/common/api.c
- [ ] workspace/all/minui/minui.c
- [ ] workspace/all/minarch/minarch.c
- [ ] Integration tests for full launch workflow

## Continuous Integration

Tests run automatically on:
- Pre-commit hooks (optional)
- Pull request validation
- Release builds

Configure CI to run:
```bash
make -f Makefile.qa lint
make -f Makefile.qa test
```

## Debugging Test Failures

### Use lldb/gdb
```bash
# Compile with debug symbols
gcc -g -o tests/unit_tests_debug ...

# Debug
lldb tests/unit_tests_debug
(lldb) run
(lldb) bt  # backtrace when crash occurs
```

### Verbose Output
```bash
# Unity verbose mode
./tests/unit_tests -v
```

### Run Single Test
```bash
# Filter by test name
./tests/unit_tests -n test_prefixMatch_exact
```

## References

- **Unity Framework**: https://github.com/ThrowTheSwitch/Unity
- **Test Organization Best Practices**: See this README's structure section
- **C Testing Tutorial**: support/unity/README.md

## Questions?

If you're adding tests and need help:
1. Look at existing tests in `unit/all/common/test_utils.c`
2. Check Unity documentation in `support/unity/`
3. Ask in discussions or open an issue
