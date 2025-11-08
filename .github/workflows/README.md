# GitHub Actions CI Workflows

This directory contains the continuous integration workflows for MinUI.

## Workflows

### Quality Assurance (`qa.yml`)

Runs on every push and pull request to `main` and `develop` branches.

**Jobs:**
- **Lint** - Runs all linting checks (cppcheck, clang-format, shellcheck)
- **Test** - Runs comprehensive test suite in Debian Buster Docker container (GCC 8.3.0)

All QA jobs run on standard `ubuntu-latest` runners in parallel.

### Platform Builds (`build.yml`)

Validates compilation works (compile-only, no artifacts).

**Platform tested:** rg35xxplus (Anbernic RG35XX Plus)

**Note:** This only tests compilation (`make PLATFORM=rg35xxplus build`), not full builds with system file copying. This catches compilation errors quickly without needing the full skeleton setup.

## Running Workflows Locally

### QA Checks

You can run the **exact same commands** locally that run in CI:

```bash
# Run all linting checks (cppcheck, format-check, shellcheck)
make lint

# Run all tests (uses Docker)
make test
```

Individual targets are also available if needed:
```bash
make lint-code      # Just cppcheck
make format-check   # Just formatting
make lint-shell     # Just shellcheck
```

### Platform Builds

```bash
# Build for a specific platform
make PLATFORM=miyoomini build
make PLATFORM=miyoomini system
make PLATFORM=miyoomini cores

# Build everything for all platforms
make all
```

## CI Strategy Summary

| Event | Runs | Duration | Purpose |
|-------|------|----------|---------|
| **PR/Push to main/develop** | Lint + Test + Build (rg35xxplus) | ~5-7 min | Fast feedback, catch compilation errors |

## Status Badges

Add to README.md:

```markdown
[![QA](https://github.com/nchapman/MinUI/actions/workflows/qa.yml/badge.svg)](https://github.com/nchapman/MinUI/actions/workflows/qa.yml)
[![Build](https://github.com/nchapman/MinUI/actions/workflows/build.yml/badge.svg)](https://github.com/nchapman/MinUI/actions/workflows/build.yml)
```

## Future Enhancements

### ARM64 Runners

GitHub now supports ARM64 runners which could significantly speed up builds since MinUI compiles for ARM devices. To use ARM64 runners:

1. Enable ARM64 runners in repository settings
2. Update workflow files to use `runs-on: [self-hosted, linux, arm64]` or `runs-on: ubuntu-24.04-arm64` (when available)

See: https://github.blog/news-insights/product-news/arm64-on-github-actions-powering-faster-more-efficient-build-systems/

### Additional Checks

Consider adding:
- Release builds on tags
- Automated deployment of release artifacts
- Code coverage reporting
- Performance benchmarks
- Cross-platform build matrix (all 12+ platforms)
