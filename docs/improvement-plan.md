# MinUI Code Quality Improvement Plan

**Goal:** Transform MinUI from a one-person project into a maintainable, testable, community-friendly codebase while preserving its minimalist philosophy and performance.

## Executive Summary

MinUI has ~30,000 lines of C code across 12+ platforms. Analysis reveals:
- **4,830-line minarch.c** with multiple responsibilities
- **Minimal error handling** - unchecked malloc/fopen throughout
- **No automated tests** - only manual hardware tests
- **Heavy global state** - blocks testability
- **Platform code duplication** - similar patterns across 12+ implementations

This plan provides a **phased approach** over 6-12 months to introduce tests, improve code quality, and enable community contributions **without breaking existing functionality**.

---

## Phase 1: Foundation (Weeks 1-4)

**Goal:** Establish infrastructure for quality improvements

### 1.1 Set Up Testing Infrastructure

**Tasks:**
- Add unit testing framework (Unity or MinUnit - both are minimal C frameworks)
- Create `tests/` directory structure
- Set up CI/CD with GitHub Actions
- Add code coverage reporting (gcov/lcov)

**Deliverables:**
```
tests/
├── unity/           # Unity test framework
├── utils/           # Tests for utils.c
├── mocks/           # Mock implementations
├── fixtures/        # Test data
└── README.md        # Testing guide
```

**CI Configuration:**
```yaml
# .github/workflows/test.yml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run unit tests
        run: make test
      - name: Upload coverage
        run: bash <(curl -s https://codecov.io/bash)
```

**Why:** Cannot improve what we cannot measure. Tests provide safety net for refactoring.

---

### 1.2 Code Quality Tools

**Tasks:**
- Add static analysis (cppcheck, clang-tidy)
- Set up formatting (clang-format with config)
- Add linting to CI
- Create `.editorconfig` for consistent style

**Configuration:**
```yaml
# .clang-format
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
```

**CI Integration:**
```bash
# Run on all PRs
cppcheck --enable=all --suppress=missingInclude workspace/
clang-format --dry-run -Werror workspace/**/*.c
```

**Why:** Automated checks catch issues before human review, establish consistent style.

---

### 1.3 Documentation Standards

**Tasks:**
- Create CONTRIBUTING.md
- Define code review checklist
- Document coding standards
- Create issue/PR templates

**CONTRIBUTING.md outline:**
```markdown
# Contributing to MinUI

## Code Standards
- Functions must have docstrings
- All malloc calls must be checked
- All file I/O must handle errors
- Max function length: 100 lines
- Max file length: 500 lines (exceptions documented)

## Testing Requirements
- New features need unit tests
- Bug fixes need regression tests
- PRs need >80% coverage for new code

## Review Process
1. Automated checks must pass
2. One maintainer approval required
3. All conversations resolved
```

**Why:** Clear guidelines reduce friction for new contributors.

---

## Phase 2: Quick Wins (Weeks 5-8)

**Goal:** Fix critical safety issues and add initial tests

### 2.1 Safety: Fix Unchecked Allocations

**Priority:** CRITICAL

**Tasks:**
- Audit all malloc/calloc/realloc calls (~50+ instances)
- Add error checking and cleanup
- Create safe wrapper functions

**Before:**
```c
static Array* Array_new(void) {
    Array* self = malloc(sizeof(Array));
    self->count = 0;  // CRASH if malloc failed
    self->capacity = 8;
    self->items = malloc(sizeof(void*) * self->capacity);
    return self;
}
```

**After:**
```c
static Array* Array_new(void) {
    Array* self = malloc(sizeof(Array));
    if (!self) {
        LOG_error("Failed to allocate Array\n");
        return NULL;
    }

    self->items = malloc(sizeof(void*) * 8);
    if (!self->items) {
        free(self);
        LOG_error("Failed to allocate Array items\n");
        return NULL;
    }

    self->count = 0;
    self->capacity = 8;
    return self;
}

// All callers must check:
Array* arr = Array_new();
if (!arr) {
    // Handle error
}
```

**OR use safe wrappers:**
```c
// In new file: workspace/all/common/mem.c
void* safe_malloc(size_t size, const char* context) {
    void* ptr = malloc(size);
    if (!ptr) {
        LOG_error("Allocation failed: %s (%zu bytes)\n", context, size);
        exit(1);  // Or handle gracefully
    }
    return ptr;
}

// Usage:
Array* self = safe_malloc(sizeof(Array), "Array_new");
```

**Impact:** Prevents crashes from allocation failures. ~50 locations to fix.

---

### 2.2 Safety: Standardize Error Handling

**Tasks:**
- Define error code enum
- Create error handling macros
- Convert void functions to return status codes

**Error codes:**
```c
// workspace/all/common/errors.h
typedef enum {
    ERR_OK = 0,
    ERR_MALLOC,
    ERR_FILE_OPEN,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_INVALID_ARG,
    ERR_NOT_FOUND,
    ERR_PLATFORM,
} ErrorCode;

const char* ErrorCode_toString(ErrorCode err);
```

**Error handling pattern:**
```c
// Before:
void SRAM_read(void) {
    FILE *sram_file = fopen(filename, "r");
    if (!sram_file) return;  // Silent failure
    // ...
}

// After:
ErrorCode SRAM_read(void) {
    FILE *sram_file = fopen(filename, "r");
    if (!sram_file) {
        LOG_warn("Failed to open SRAM file: %s\n", filename);
        return ERR_FILE_OPEN;
    }

    if (fread(sram, 1, size, sram_file) != size) {
        LOG_error("Failed to read SRAM data\n");
        fclose(sram_file);
        return ERR_FILE_READ;
    }

    fclose(sram_file);
    return ERR_OK;
}

// Caller:
ErrorCode err = SRAM_read();
if (err != ERR_OK) {
    // Handle or propagate
}
```

**Impact:** Makes errors visible, propagatable, testable.

---

### 2.3 Testing: Add Tests for utils.c

**Goal:** Prove testing works, build momentum

**Tasks:**
- Test all string functions (prefixMatch, suffixMatch, etc.)
- Test file path functions
- Test display name parsing
- Achieve 90%+ coverage for utils.c

**Example test:**
```c
// tests/utils/test_string.c
#include "unity.h"
#include "../../workspace/all/common/utils.h"

void setUp(void) {}
void tearDown(void) {}

void test_prefixMatch_exact(void) {
    TEST_ASSERT_TRUE(prefixMatch("hello", "hello"));
}

void test_prefixMatch_longer(void) {
    TEST_ASSERT_TRUE(prefixMatch("hello world", "hello"));
}

void test_prefixMatch_nomatch(void) {
    TEST_ASSERT_FALSE(prefixMatch("hello", "world"));
}

void test_suffixMatch_extension(void) {
    TEST_ASSERT_TRUE(suffixMatch("game.gb", ".gb"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prefixMatch_exact);
    RUN_TEST(test_prefixMatch_longer);
    RUN_TEST(test_prefixMatch_nomatch);
    RUN_TEST(test_suffixMatch_extension);
    return UNITY_END();
}
```

**Build integration:**
```makefile
# Makefile
test:
	$(CC) -o tests/utils_test tests/utils/test_string.c \
		workspace/all/common/utils.c tests/unity/unity.c
	./tests/utils_test
```

**Impact:**
- Demonstrates testing value
- Prevents regressions in utility code
- Template for future tests

---

### 2.4 Documentation: Function-Level Docs

**Tasks:**
- Document all public API functions (GFX_*, SND_*, PAD_*, PWR_*)
- Add docstrings to complex functions
- Generate HTML docs (optional: Doxygen)

**Format:**
```c
/**
 * @brief Initializes the graphics subsystem
 *
 * Sets up SDL video mode, loads UI assets, initializes fonts,
 * and prepares rendering surfaces. Must be called before any
 * other GFX_* functions.
 *
 * @return ERR_OK on success, error code on failure
 *
 * @note This function allocates resources that must be freed
 *       with GFX_quit()
 */
ErrorCode GFX_init(void);

/**
 * @brief Loads a PNG image from disk
 *
 * @param path File path to PNG image (must exist)
 * @return Loaded surface on success, NULL on failure
 *
 * @note Returned surface is cached internally - do NOT free.
 *       Call GFX_freeImage() to release if needed.
 *
 * @warning Path must remain valid for lifetime of image
 */
SDL_Surface* GFX_loadImage(const char* path);
```

**Impact:** Makes codebase approachable for new contributors.

---

## Phase 3: Modularization (Weeks 9-16)

**Goal:** Break up large files, reduce coupling

### 3.1 Split minarch.c (4,830 lines → ~5 files)

**Target structure:**
```
workspace/all/minarch/
├── minarch.c          # Main entry point, game loop (500 lines)
├── core.c/h           # Libretro core loading/callbacks (800 lines)
├── savestate.c/h      # Save states, SRAM, RTC (400 lines)
├── config.c/h         # Configuration system (600 lines)
├── video.c/h          # Video pipeline, scaling (800 lines)
├── audio.c/h          # Audio buffering (300 lines)
├── input.c/h          # Input mapping, shortcuts (400 lines)
├── menu.c/h           # In-game menu (600 lines)
├── disc.c/h           # Disc swapping, M3U (200 lines)
└── makefile           # Updated build
```

**Migration strategy:**
1. Create new files with stubs
2. Move functions one at a time
3. Keep minarch.c compiling after each move
4. Update makefile
5. Test on real hardware

**Example split - core.c:**
```c
// minarch/core.h
typedef struct {
    void* handle;
    struct retro_core_t {
        // Core function pointers
    } api;
} Core;

ErrorCode Core_init(void);
ErrorCode Core_load(const char* core_path);
ErrorCode Core_loadGame(const char* game_path);
void Core_run(void);
void Core_unload(void);
void Core_quit(void);

// minarch/core.c
#include "core.h"

static Core g_core = {0};

ErrorCode Core_load(const char* core_path) {
    g_core.handle = dlopen(core_path, RTLD_LAZY);
    if (!g_core.handle) {
        LOG_error("Failed to load core: %s\n", dlerror());
        return ERR_FILE_OPEN;
    }
    // Load symbols...
    return ERR_OK;
}
```

**Impact:**
- Easier to understand (500 lines vs 4,830)
- Easier to test (can test savestate.c independently)
- Enables parallel development

---

### 3.2 Extract Common Platform Code

**Problem:** 12 platforms with duplicated code

**Solution:** Create platform utility library

**New structure:**
```
workspace/common/platform/
├── platform_common.c/h    # Shared platform code
├── video_common.c/h       # Common video setup
├── input_common.c/h       # Common input patterns
├── scaler_common.c/h      # Common scaling logic
└── battery_common.c/h     # Common battery reading
```

**Example - video_common.c:**
```c
// Before: Duplicated in 12 platforms
SDL_Surface* PLAT_initVideo(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(0);
    // Platform-specific parts...
}

// After: Shared code + hooks
typedef struct {
    int width;
    int height;
    int bpp;
    void (*setup_hook)(void);  // Platform-specific
} VideoConfig;

SDL_Surface* Platform_initVideoCommon(VideoConfig* config) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(0);

    if (config->setup_hook) {
        config->setup_hook();
    }

    return SDL_SetVideoMode(
        config->width,
        config->height,
        config->bpp,
        SDL_SWSURFACE
    );
}

// Each platform:
static void miyoomini_video_setup(void) {
    // Miyoo Mini specific setup
}

SDL_Surface* PLAT_initVideo(void) {
    VideoConfig config = {
        .width = 640,
        .height = 480,
        .bpp = 16,
        .setup_hook = miyoomini_video_setup
    };
    return Platform_initVideoCommon(&config);
}
```

**Impact:**
- Reduce duplication from ~500 lines × 12 = 6,000 lines to ~500 + (12 × 50) = 1,100 lines
- Easier to add new platforms
- Bugs fixed once, not 12 times

---

### 3.3 Introduce Dependency Injection

**Goal:** Make code testable by inverting dependencies

**Example - File I/O abstraction:**
```c
// Before (in minui.c, api.c, minarch.c):
FILE* f = fopen(path, "r");
if (!f) return;
fread(buffer, size, 1, f);
fclose(f);

// After: workspace/all/common/filesystem.h
typedef struct {
    void* (*open)(const char* path, const char* mode);
    size_t (*read)(void* ptr, size_t size, size_t count, void* file);
    int (*close)(void* file);
    // ... etc
} FileSystem;

// Real implementation: workspace/all/common/filesystem_real.c
static void* fs_real_open(const char* path, const char* mode) {
    return fopen(path, mode);
}

FileSystem* FileSystem_real(void) {
    static FileSystem fs = {
        .open = fs_real_open,
        .read = (void*)fread,
        .close = (void*)fclose,
    };
    return &fs;
}

// Test implementation: tests/mocks/filesystem_mock.c
static void* fs_mock_open(const char* path, const char* mode) {
    // Return mock file handle
}

FileSystem* FileSystem_mock(void) {
    static FileSystem fs = {
        .open = fs_mock_open,
        // ...
    };
    return &fs;
}

// Usage in code:
void loadConfig(FileSystem* fs) {
    void* file = fs->open("config.ini", "r");
    // ...
}

// Production:
loadConfig(FileSystem_real());

// Test:
loadConfig(FileSystem_mock());
```

**Impact:** Enables unit testing without real files/hardware.

---

## Phase 4: Comprehensive Testing (Weeks 17-24)

**Goal:** Achieve 60%+ code coverage

### 4.1 Unit Tests for Core Logic

**Targets:**
- Array/Hash operations (minui.c)
- Config parsing (minarch.c)
- Display name parsing (minui.c)
- Input mapping (api.c)
- Text wrapping/truncation (api.c)

**Example - Array tests:**
```c
// tests/minui/test_array.c
void test_Array_push_increases_count(void) {
    Array* arr = Array_new();
    TEST_ASSERT_EQUAL(0, arr->count);

    Array_push(arr, (void*)1);
    TEST_ASSERT_EQUAL(1, arr->count);

    Array_free(arr);
}

void test_Array_push_expands_capacity(void) {
    Array* arr = Array_new();
    // Push 100 items
    for (int i = 0; i < 100; i++) {
        Array_push(arr, (void*)(long)i);
    }

    TEST_ASSERT_EQUAL(100, arr->count);
    TEST_ASSERT_GREATER_OR_EQUAL(100, arr->capacity);
    Array_free(arr);
}
```

**Coverage target:** 80% for utils.c, 60% for minui/minarch business logic.

---

### 4.2 Integration Tests

**Goal:** Test component interactions without hardware

**Approach:**
- Mock SDL, platform functions
- Test minui navigation logic
- Test minarch save/load states
- Test config persistence

**Example - SaveState integration test:**
```c
// tests/integration/test_savestate.c
void test_savestate_roundtrip(void) {
    // Setup mock filesystem
    FileSystem* fs = FileSystem_mock();

    // Create fake save state
    uint8_t save_data[1024] = {1, 2, 3, 4, 5};

    // Save
    ErrorCode err = SaveState_write(fs, "test.st0", save_data, 1024);
    TEST_ASSERT_EQUAL(ERR_OK, err);

    // Load
    uint8_t load_data[1024] = {0};
    err = SaveState_read(fs, "test.st0", load_data, 1024);
    TEST_ASSERT_EQUAL(ERR_OK, err);

    // Verify
    TEST_ASSERT_EQUAL_MEMORY(save_data, load_data, 1024);
}
```

---

### 4.3 Platform Tests

**Goal:** Automated tests for each platform (where possible)

**Approach:**
- Hardware tests in CI (if runners available)
- OR: Platform simulation (QEMU for ARM platforms)
- OR: Mock platform functions for logic tests

**Platform test structure:**
```
tests/platforms/
├── miyoomini/
│   ├── test_battery.c
│   ├── test_brightness.c
│   └── test_input.c
├── rg35xx/
│   └── ...
└── common/
    └── test_platform_common.c
```

**Example:**
```c
// tests/platforms/miyoomini/test_battery.c
void test_battery_reading_range(void) {
    int level = PLAT_getBatteryStatus();
    TEST_ASSERT_GREATER_OR_EQUAL(-1, level);
    TEST_ASSERT_LESS_OR_EQUAL(200, level);  // 0-100 normal, 101-200 charging
}
```

---

## Phase 5: Continuous Improvement (Weeks 25+)

**Goal:** Establish sustainable practices

### 5.1 CI/CD Pipeline

**Full pipeline:**
```yaml
name: CI/CD

on: [push, pull_request]

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run clang-format
        run: make format-check
      - name: Run cppcheck
        run: make lint

  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Unit tests
        run: make test
      - name: Coverage
        run: make coverage
      - name: Upload to codecov
        uses: codecov/codecov-action@v2

  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        platform: [miyoomini, rg35xx, rg35xxplus]
    steps:
      - uses: actions/checkout@v2
      - name: Build ${{ matrix.platform }}
        run: make PLATFORM=${{ matrix.platform }}
      - name: Upload artifacts
        uses: actions/upload-artifact@v2

  integration:
    runs-on: ubuntu-latest
    needs: build
    steps:
      - name: Integration tests
        run: make test-integration
```

**Benefits:**
- Every PR is tested automatically
- Catches issues before merge
- Builds all platforms on every change

---

### 5.2 Code Review Standards

**Required for all PRs:**
- [ ] All tests pass
- [ ] Coverage >80% for new code
- [ ] Lint checks pass
- [ ] Documentation updated
- [ ] Changelog entry added
- [ ] One maintainer approval

**Review checklist:**
```markdown
## Code Review Checklist

### Functionality
- [ ] Code does what it claims
- [ ] No obvious bugs
- [ ] Error cases handled

### Quality
- [ ] Functions <100 lines
- [ ] No duplicated code
- [ ] Clear variable names
- [ ] Comments explain "why", not "what"

### Testing
- [ ] Unit tests added
- [ ] Tests cover edge cases
- [ ] Tests are deterministic

### Safety
- [ ] All malloc/fopen checked
- [ ] No buffer overflows
- [ ] No memory leaks
```

---

### 5.3 Refactoring Backlog

**Ongoing improvements:**

**Priority 1:**
- [ ] Split minui.c into modules (1,704 → ~500 lines)
- [ ] Extract platform common code
- [ ] Add error codes throughout

**Priority 2:**
- [ ] Reduce global state in minarch.c
- [ ] Create save state module
- [ ] Separate config system

**Priority 3:**
- [ ] Optimize scaler (profile-guided)
- [ ] Add benchmarks
- [ ] Document platform porting guide

---

## Success Metrics

### Month 3:
- [x] Test infrastructure in place
- [x] CI/CD running
- [x] 100% of allocations checked
- [x] utils.c at 90% coverage
- [x] CONTRIBUTING.md published

### Month 6:
- [x] minarch.c split into modules
- [x] Platform common code extracted
- [x] 40% overall code coverage
- [x] 5+ community PRs merged

### Month 12:
- [x] 60% overall code coverage
- [x] All major components tested
- [x] Zero known crashes
- [x] 20+ community contributors
- [x] Monthly releases

---

## Risk Mitigation

### Risk 1: Breaking Existing Functionality

**Mitigation:**
- Comprehensive smoke tests on hardware before merging
- Beta testing channel for early adopters
- Easy rollback (keep old builds available)
- Automated testing catches regressions

### Risk 2: Community Resistance to Changes

**Mitigation:**
- Communicate plan publicly
- Accept feedback and adjust
- Maintain backward compatibility
- Document all breaking changes

### Risk 3: Testing Takes Too Long

**Mitigation:**
- Start with quick wins (utils.c)
- Focus on high-value tests
- Don't aim for 100% coverage
- Prioritize new code coverage

### Risk 4: Refactoring Introduces Bugs

**Mitigation:**
- Refactor in small increments
- Keep tests passing at each step
- Extensive testing on real hardware
- Ship refactorings separately from features

---

## Resource Requirements

### Development Time

**Assuming 1-2 people part-time:**
- **Phase 1:** 40-60 hours (setup)
- **Phase 2:** 80-100 hours (quick wins)
- **Phase 3:** 120-160 hours (modularization)
- **Phase 4:** 100-140 hours (comprehensive testing)
- **Phase 5:** Ongoing (~20 hours/month)

**Total:** ~400-500 hours over 6-12 months

### Infrastructure

**Free tier options:**
- GitHub Actions (2,000 minutes/month free)
- Codecov (free for open source)
- GitHub (free for public repos)

**Optional paid:**
- Hardware test runners (~$200 for Raspberry Pi + devices)
- Cloud build machines (~$50/month)

---

## Getting Started

### Week 1 Action Items

1. **Set up testing framework:**
   ```bash
   cd MinUI
   mkdir -p tests/unity
   wget https://github.com/ThrowTheSwitch/Unity/releases/download/v2.5.2/unity.c
   wget https://github.com/ThrowTheSwitch/Unity/releases/download/v2.5.2/unity.h
   ```

2. **Create first test:**
   ```bash
   # tests/test_utils.c
   # (see example above)
   ```

3. **Add to Makefile:**
   ```makefile
   test: tests/test_utils
   	./tests/test_utils

   tests/test_utils: tests/test_utils.c workspace/all/common/utils.c
   	$(CC) -o $@ $^ tests/unity/unity.c -I. -Iworkspace/all
   ```

4. **Run:**
   ```bash
   make test
   ```

5. **Set up GitHub Actions:**
   ```bash
   mkdir -p .github/workflows
   # Create test.yml (see CI example above)
   ```

6. **Commit and push:**
   ```bash
   git checkout -b feature/testing-infrastructure
   git add tests/ .github/workflows/test.yml Makefile
   git commit -m "Add testing infrastructure and first tests"
   git push origin feature/testing-infrastructure
   ```

---

## Conclusion

This plan balances **immediate safety improvements** (unchecked allocations) with **long-term sustainability** (modularization, testing). By following this phased approach:

1. **Foundation** - Establish tooling
2. **Quick Wins** - Fix critical issues, prove value
3. **Modularization** - Make code maintainable
4. **Testing** - Comprehensive coverage
5. **Continuous** - Sustainable practices

MinUI can evolve from a solo project to a community-maintained codebase **without sacrificing its minimalist philosophy or breaking existing functionality**.

The key is to **start small** (utils.c tests), **ship incrementally** (one module at a time), and **always keep the lights on** (tests prevent regressions).

**Next Steps:** Review this plan, adjust priorities based on your goals, and start with Week 1 action items.
