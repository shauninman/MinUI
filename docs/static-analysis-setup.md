# Static Analysis Setup

MinUI uses `cppcheck` for static code analysis to catch bugs early.

## Installation

### macOS
```bash
brew install cppcheck
```

### Linux
```bash
sudo apt-get install cppcheck
```

## Usage

### Quick Check (Recommended)
```bash
make -f Makefile.qa lint
```

Checks `workspace/all/` (common code) for serious issues:
- NULL pointer dereferences
- Memory leaks
- Buffer overflows
- Uninitialized variables

### Full Check (Verbose)
```bash
make -f Makefile.qa lint-full
```

Checks entire workspace including platform-specific code.

### Generate Report
```bash
make -f Makefile.qa report
```

Creates `cppcheck-report.txt` with detailed findings.

## Current Findings

### Real Issue Found ✅

**Undefined Behavior in api.c:355**
```c
uint32_t of = ((sum < c1) | (ret < sum)) << 31;
```

**Problem:** Shifting signed 32-bit value by 31 bits is undefined behavior in C.

**Impact:** May cause incorrect calculations on some platforms.

**Fix:** Use unsigned types:
```c
uint32_t of = ((uint32_t)((sum < c1) | (ret < sum))) << 31;
```

**Location:** workspace/all/common/api.c:355

### Suppressed Issues

The following are suppressed as they're not actual problems:

1. **unknownMacro warnings** - `PLATFORM`, `PAKS_PATH`, etc. are defined during compilation
2. **libretro-common warnings** - Third-party code we don't maintain
3. **normalCheckLevelMaxBranches** - Informational only, not errors

## Configuration

### `.cppcheck-suppressions`

Controls what gets ignored:

```
# Third-party code
*:*/libretro-common/*

# Platform macros (defined at compile time)
unknownMacro:*PLATFORM*

# Noise reduction
normalCheckLevelMaxBranches
unusedFunction
```

### `Makefile.qa`

Defines check targets and flags:

```makefile
CPPCHECK_FLAGS = --enable=warning \
                 --suppressions-list=.cppcheck-suppressions \
                 --error-exitcode=0 \
                 --quiet
```

## Adding to Workflow

### Before Committing
```bash
make -f Makefile.qa lint
```

### Before PR
```bash
make -f Makefile.qa lint-full
```

## Tightening Rules Over Time

Currently using **forgiving rules** to avoid overwhelming output:
- Only `--enable=warning` (not style, performance, portability)
- Suppressions for third-party code
- No CI enforcement yet

### Future Improvements

As code quality improves, gradually enable:

1. **Phase 2:** Add `--enable=style` for coding style issues
2. **Phase 3:** Add `--enable=performance` for optimization hints
3. **Phase 4:** Add `--enable=portability` for cross-platform issues
4. **Phase 5:** Make lint checks mandatory in CI (fail on errors)

## Next Steps

1. ✅ **Static analysis set up** - You are here
2. ⏭️ **Fix the real issue** - Fix api.c:355 undefined behavior
3. ⏭️ **Set up tests** - Add Unity framework
4. ⏭️ **Write first tests** - Test utils.c functions

## Known Limitations

- Doesn't understand platform-specific macros (PLATFORM, PAKS_PATH, etc.)
- Can't check platform-specific code without platform definitions
- Third-party code (libretro-common) generates many warnings

These are acceptable trade-offs for a lightweight setup.
