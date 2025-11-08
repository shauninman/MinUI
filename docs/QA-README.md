# MinUI Quality Assurance

Quick reference for code quality tools.

## Quick Start

```bash
# Run static analysis
make -f Makefile.qa lint

# Run tests
make -f Makefile.qa test

# Check code formatting (safe, no changes)
make -f Makefile.qa format-check

# Run all checks
make -f Makefile.qa lint test format-check
```

## What's Set Up

### ✅ Static Analysis (cppcheck)
- **Tool:** cppcheck
- **Rules:** Forgiving (warnings only, no style checks yet)
- **Scope:** workspace/all/ (common code)
- **Issues Found:** 1 real issue (undefined behavior in api.c:355)
- **Doc:** [static-analysis-setup.md](static-analysis-setup.md)

### ✅ Unit Tests (Unity)
- **Framework:** Unity (lightweight C test framework)
- **Coverage:** 9 tests for string utilities
- **Status:** All passing ✓
- **Doc:** [testing-setup.md](testing-setup.md)

### ✅ Code Formatting (clang-format)
- **Tool:** clang-format
- **Style:** Tabs, K&R braces, 100 char lines
- **Status:** Available (use on new/modified code)
- **Doc:** [formatting-setup.md](formatting-setup.md)

## Current Status

| Item | Status | Next Step |
|------|--------|-----------|
| Static analysis | ✅ Working | Fix api.c:355 issue |
| Test infrastructure | ✅ Working | Add more tests |
| Test coverage | 🟡 Minimal | Test more functions |
| CI/CD | ❌ Not set up | Add GitHub Actions (later) |

## Files Added

```
.cppcheck-suppressions      # What to ignore in static analysis
Makefile.qa                 # QA commands (lint, test)
tests/unity/                # Unity test framework
tests/utils/                # Test files
docs/static-analysis-setup.md
docs/testing-setup.md
docs/QA-README.md          # This file
```

## Philosophy

**Start gentle, tighten gradually:**

1. ✅ **Phase 1 (Now):** Forgiving rules, find serious issues
2. ⏭️ **Phase 2:** Fix found issues, add more tests
3. ⏭️ **Phase 3:** Enable stricter rules (style, performance)
4. ⏭️ **Phase 4:** Enforce in CI

## Real Issue Found

**api.c:355 - Undefined Behavior**
```c
uint32_t of = ((sum < c1) | (ret < sum)) << 31;
```

Shifting signed 32-bit value by 31 bits is undefined.

**Impact:** May cause incorrect calculations.

**Priority:** Medium (should fix)

## Commands Reference

```bash
# Static analysis
make -f Makefile.qa lint           # Quick check (common code)
make -f Makefile.qa lint-full      # Full check (all code)
make -f Makefile.qa report         # Generate report file

# Testing
make -f Makefile.qa test           # Run all tests

# Cleanup
make -f Makefile.qa clean-tests    # Clean test artifacts
make -f Makefile.qa clean-qa       # Clean all QA artifacts

# Help
make -f Makefile.qa help           # Show all commands
```

## Integration with Development

### Before Committing
```bash
make -f Makefile.qa lint test
```

### When Refactoring
```bash
# 1. Write tests first
# 2. Refactor code
# 3. Run tests to verify
make -f Makefile.qa test
```

### When Adding Features
```bash
# 1. Run lint to check new code
make -f Makefile.qa lint

# 2. Add tests for new functionality
# 3. Verify tests pass
make -f Makefile.qa test
```

## Next Steps

### Immediate
1. Fix api.c:355 undefined behavior
2. Add tests for more utils functions
3. Document safe coding patterns

### Near Term (1-2 months)
4. Add tests for config parsing
5. Add tests for save state logic
6. Enable style checking in cppcheck

### Long Term (3-6 months)
7. Increase test coverage to 40%+
8. Add integration tests
9. Set up CI/CD
10. Make lint/test mandatory for PRs

## Resources

- [Static Analysis Setup](static-analysis-setup.md) - Detailed cppcheck guide
- [Testing Setup](testing-setup.md) - Detailed testing guide
- [Improvement Plan](improvement-plan.md) - Full quality improvement roadmap
- [Unity Framework](https://github.com/ThrowTheSwitch/Unity) - Test framework docs

## Success Metrics

### Current
- ✅ Static analysis running
- ✅ 9 tests passing
- ✅ 1 real issue found

### 1 Month
- ⏭️ Real issue fixed
- ⏭️ 20+ tests
- ⏭️ Test more modules

### 3 Months
- ⏭️ 100+ tests
- ⏭️ CI running
- ⏭️ Style checking enabled
