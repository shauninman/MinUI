# MinUI Testing Checklist

This document provides a prioritized checklist of files to test, ordered by importance, complexity, and dependencies.

## Testing Philosophy

1. **Test common utilities first** - These are used everywhere and bugs cascade
2. **Test pure functions** - Functions with no side effects are easiest to test
3. **Test platform-independent code** - Avoid testing platform-specific implementations
4. **Mock external dependencies** - Use stubs for SDL, hardware, etc.
5. **Skip third-party code** - Don't test libretro-common (maintained externally)

## Priority Order

### ✅ Phase 1: Foundation (COMPLETED)
**Status:** All tests passing

| File | Lines | Priority | Status | Tests | Notes |
|------|-------|----------|--------|-------|-------|
| `workspace/all/common/utils.c` | 219 | **CRITICAL** | ✅ DONE | 52 | String manipulation, file I/O, timing - used everywhere |

**Key achievements:**
- Found and fixed critical memory overlap bug in `getEmuName()`
- Fixed sign comparison warning in `exactMatch()`
- Comprehensive test coverage of all public functions
- Established test infrastructure with Unity framework

---

### 🎯 Phase 2: Core Utilities (NEXT)
**Rationale:** Test remaining utility code before testing code that depends on it

| File | Lines | Priority | Status | Tests | Testability |
|------|-------|----------|--------|-------|-------------|
| `workspace/all/common/api.c` | 1,722 | **HIGH** | ⏳ TODO | 0 | Medium - needs SDL/TTF mocks |
| `workspace/all/clock/clock.c` | 322 | **MEDIUM** | ⏳ TODO | 0 | High - mostly pure logic |
| `workspace/all/say/say.c` | 52 | **LOW** | ⏳ TODO | 0 | High - very simple |
| `workspace/all/minput/minput.c` | 274 | **MEDIUM** | ⏳ TODO | 0 | Medium - input handling |
| `workspace/all/syncsettings/syncsettings.c` | 12 | **LOW** | ⏳ TODO | 0 | High - trivial |

**Recommended order:**
1. `syncsettings.c` - Simplest, good warm-up
2. `say.c` - Small and simple
3. `clock.c` - Pure logic, no external dependencies
4. `minput.c` - Input handling with some complexity
5. `api.c` - Complex, many SDL dependencies (tackle in parts)

---

### 🔧 Phase 3: Application Layer
**Rationale:** Test after all utilities are tested and stable

| File | Lines | Priority | Status | Tests | Testability |
|------|-------|----------|--------|-------|-------------|
| `workspace/all/minui/minui.c` | 1,704 | **HIGH** | ⏳ TODO | 0 | Low - heavy SDL/file dependencies |
| `workspace/all/minarch/minarch.c` | 4,830 | **HIGH** | ⏳ TODO | 0 | Very Low - libretro integration |

**Notes:**
- These are more suitable for **integration tests** than unit tests
- Consider testing specific helper functions in isolation
- Mock libretro core for minarch testing
- Mock SDL surfaces and TTF for minui testing

---

### 🎨 Phase 4: Graphics (OPTIONAL)
**Rationale:** Low-level pixel manipulation - consider visual/integration testing instead

| File | Lines | Priority | Status | Tests | Testability |
|------|-------|----------|--------|-------|-------------|
| `workspace/all/common/scaler.c` | 3,072 | **MEDIUM** | ⏳ TODO | 0 | Medium - pure pixel math |

**Notes:**
- Contains NEON assembly optimizations - hard to unit test
- Could test C fallback implementations
- Better suited for visual regression tests
- Low priority unless bugs are found

---

## Detailed Test Plans

### api.c - Core API Functions
**Lines:** 1,722 | **Priority:** HIGH | **Complexity:** HIGH

#### Testable Functions (grouped by subsystem)

**Logging (Simple - Test First)**
- `LOG_note()` - Basic logging with different levels

**Text Utilities (Medium)**
- `GFX_truncateText()` - Text truncation with max width
- `GFX_wrapText()` - Text wrapping logic
- `GFX_getButtonWidth()` - Calculate button width

**Rendering Helpers (Complex - Mock SDL)**
- `GFX_blitAsset()` - Blit assets to surface
- `GFX_blitPill()` - Render pill-shaped UI elements
- `GFX_blitRect()` - Render rectangles
- `GFX_blitBattery()` - Render battery indicator
- `GFX_blitButton()` - Render button with hint
- `GFX_blitMessage()` - Render text message

**Math/Utility (Simple)**
- `average16()` / `average32()` - Color averaging (inline)
- `gcd()` - Greatest common divisor (inline)

**Strategy:**
1. Start with logging and math utilities (no SDL needed)
2. Mock TTF_Font for text utilities
3. Mock SDL_Surface for rendering functions
4. Test each subsystem independently

---

### clock.c - Clock Application
**Lines:** 322 | **Priority:** MEDIUM | **Complexity:** LOW

**Expected Functions:**
- Time/date display formatting
- Time zone handling
- Clock rendering logic

**Strategy:**
- Mock SDL for rendering
- Mock time functions for deterministic testing
- Test time formatting edge cases (midnight, DST changes)

---

### say.c - Simple Text-to-Speech Wrapper
**Lines:** 52 | **Priority:** LOW | **Complexity:** LOW

**Strategy:**
- Likely just wraps external TTS command
- Test command construction
- Mock system() calls

---

### minput.c - Input Configuration
**Lines:** 274 | **Priority:** MEDIUM | **Complexity:** MEDIUM

**Expected Functions:**
- Input mapping configuration
- Button combination handling
- Controller profile management

**Strategy:**
- Test input mapping logic
- Test button combination detection
- Mock SDL input events

---

### syncsettings.c - Settings Synchronization
**Lines:** 12 | **Priority:** LOW | **Complexity:** TRIVIAL

**Strategy:**
- With only 12 lines, likely a simple wrapper
- Test basic functionality
- Quick win for test coverage

---

## Test Infrastructure Needs

### Current Infrastructure ✅
- Unity test framework
- Platform stubs (`tests/support/platform.h`)
- Mirror directory structure
- Makefile.qa integration

### Needed Mocks
- [ ] SDL_Surface mock
- [ ] TTF_Font mock
- [ ] SDL_Rect utilities
- [ ] File I/O mocking (for deterministic tests)
- [ ] Time mocking (for clock tests)

### Future Enhancements
- [ ] Code coverage reporting (gcov/lcov)
- [ ] Memory leak detection (valgrind)
- [ ] Continuous integration tests
- [ ] Integration test framework

---

## Testing Metrics

### Current Coverage
```
Total testable files: 8
Files tested: 1 (12.5%)
Total tests: 52
Lines tested: ~219 / ~8,200 (2.7%)
```

### Target Coverage
```
Phase 1 (Foundation): 100% ✅
Phase 2 (Core Utilities): 80% target
Phase 3 (Application): 60% target (integration tests)
Phase 4 (Graphics): 40% target (optional)
```

---

## Recommended Next Steps

1. **Immediate (Week 1-2):**
   - Test `syncsettings.c` (12 lines - quick win)
   - Test `say.c` (52 lines - simple)
   - Test `clock.c` (322 lines - no complex dependencies)

2. **Short-term (Week 3-4):**
   - Create SDL/TTF mocks
   - Test logging functions in `api.c`
   - Test text utilities in `api.c`
   - Test `minput.c`

3. **Medium-term (Month 2):**
   - Test remaining `api.c` functions (with mocks)
   - Begin integration tests for `minui.c`
   - Begin integration tests for `minarch.c`

4. **Long-term (Month 3+):**
   - Add code coverage reporting
   - Set up CI/CD pipeline
   - Consider `scaler.c` testing or visual regression tests
   - Performance testing for critical paths

---

## Notes

- **Skip libretro-common**: This is third-party code maintained externally
- **Focus on business logic**: Test algorithms and logic, not just I/O
- **Mock external dependencies**: Don't test SDL, test your use of SDL
- **Integration tests matter**: Some code is better tested end-to-end
- **Bug-driven testing**: When bugs are found, add regression tests

---

## Questions to Answer

Before testing each file, answer:

1. **What does this file do?** (Read first 50 lines and skim)
2. **What are the public functions?** (Check header file)
3. **What are the dependencies?** (External libs, other modules)
4. **Can we test in isolation?** (Or do we need mocks/stubs)
5. **What are the edge cases?** (NULL, empty, overflow, underflow)
6. **What are the critical paths?** (Most frequently called)
7. **What could break?** (Memory, bounds, race conditions)

This checklist will evolve as we learn more about each file!
