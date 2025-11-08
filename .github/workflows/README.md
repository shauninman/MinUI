# GitHub Actions CI Workflows

This directory contains the continuous integration workflows for MinUI.

## Workflows

### Quality Assurance (`qa.yml`)

Runs on every push and pull request to `main` and `develop` branches.

**Jobs:**
- **Unit Tests** - Runs comprehensive test suite in Debian Buster Docker container (GCC 8.3.0)
- **Static Analysis** - cppcheck on workspace/all/ (platform-independent code)
- **Code Formatting** - Validates code formatting with clang-format
- **Shell Script Linting** - shellcheck on all shell scripts

All QA jobs run on standard `ubuntu-latest` runners.

### Platform Builds (`build.yml`)

Validates that builds work for multiple platforms.

**Default platforms tested on every PR:**
- miyoomini (Miyoo Mini)
- trimuismart (Trimui Smart)
- rg35xxplus (Anbernic RG35XX Plus)

**Workflow dispatch:**
Can manually trigger builds for specific platforms or all platforms using the "Run workflow" button in GitHub Actions.

**Artifacts:**
Build artifacts are uploaded and retained for 7 days for testing.

## Running Workflows Locally

### QA Checks

You can run the same QA checks locally that run in CI:

```bash
# Run all tests (uses Docker)
make test

# Run static analysis
make lint

# Check code formatting
make format-check

# Lint shell scripts
make -f Makefile.qa lint-shell
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
