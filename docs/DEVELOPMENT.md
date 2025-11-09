# MinUI Development Guide

This guide covers building MinUI, running tests, and contributing code.

## Building MinUI

MinUI uses Docker to cross-compile for ARM devices. You don't need the actual hardware to build.

### Prerequisites

- Docker Desktop
- Make (pre-installed on macOS/Linux)
- Git

### Quick Start

Build everything:
```bash
make
```

Build for one platform:
```bash
make PLATFORM=miyoomini
```

Available platforms: `miyoomini`, `my282`, `my355`, `trimuismart`, `rg35xx`, `rg35xxplus`, `rgb30`, `tg5040`, `m17`, `gkdpixel`, `magicmini`, `zero28`

### Platform Shell (for development)

Drop into a build environment for interactive development:
```bash
make PLATFORM=miyoomini shell
```

Inside the container you can build components individually:
```bash
cd /root/workspace/all/minui
make
```

## Code Quality

### Run All Checks

```bash
make test        # Run unit tests
make lint        # Static analysis
make format      # Auto-format code
```

### Static Analysis

Find bugs before they ship:
```bash
make lint        # Check common code
```

This runs `cppcheck` on `workspace/all/` which contains all the platform-independent code.

### Unit Tests

MinUI uses Unity for testing:
```bash
make test
```

Tests live in `tests/unit/` and mirror the structure of `workspace/all/`. For example, `workspace/all/common/utils.c` has tests in `tests/unit/all/common/test_utils.c`.

#### Writing Tests

Here's an example test:
```c
#include "unity.h"
#include "utils.h"

void test_getDisplayName_stripsExtension(void) {
    char result[256];
    getDisplayName("game.gb", result);
    TEST_ASSERT_EQUAL_STRING("game", result);
}
```

Add your test function to the `main()` function in your test file:
```c
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_getDisplayName_stripsExtension);
    return UNITY_END();
}
```

Run tests: `make test`

### Code Formatting

MinUI uses `clang-format` with tabs and K&R-style braces:
```bash
make format            # Format all code
make format-check      # Check without changing
```

Settings in `.clang-format`:
- Tabs for indentation (width: 4)
- Opening braces on same line
- Left-aligned pointers (`char* name`)
- 100 character line limit

### Shell Script Linting

Check shell scripts with `shellcheck`:
```bash
make lint-shell
```

## Project Structure

```
MinUI/
├── workspace/
│   ├── all/              # Platform-independent code
│   │   ├── minui/        # Launcher
│   │   ├── minarch/      # Libretro frontend
│   │   └── common/       # Shared utilities and API
│   │
│   └── <platform>/       # Platform-specific code
│       ├── platform/     # Hardware definitions
│       ├── keymon/       # Button monitoring daemon
│       ├── libmsettings/ # Settings library
│       └── cores/        # Libretro cores
│
├── skeleton/             # Files copied to SD card
│   ├── SYSTEM/res/       # Shared assets (fonts, sprites)
│   └── BOOT/             # Boot scripts
│
├── build/                # Build output (generated)
├── tests/                # Unit tests
└── docs/                 # Documentation
```

## Platform Architecture

MinUI uses a **platform abstraction layer** so one codebase supports 20+ devices:

**Common code** (`workspace/all/`) calls abstract APIs like:
- `GFX_clear()` - Clear screen
- `PAD_poll()` - Read buttons
- `PWR_getBatteryLevel()` - Get battery %

**Platform code** (`workspace/<platform>/platform/`) implements these for specific hardware:
- `platform.h` - Hardware constants (screen size, button codes)
- `platform.c` - Hardware functions (if needed)

Example from `platform.h`:
```c
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define BUTTON_A SDLK_SPACE
```

The common code compiles once, then links with different platform definitions for each device.

## Adding Features

### Making Changes to Common Code

1. Edit files in `workspace/all/`
2. Write tests in `tests/unit/`
3. Run `make test` to verify
4. Format with `make format`
5. Check with `make lint`

### Adding Platform-Specific Features

1. Edit `workspace/<platform>/platform/` files
2. Test on actual hardware (cross-compilation can't verify hardware behavior)
3. Document in `workspace/<platform>/README.md`

### Testing Your Changes

Build for a specific platform:
```bash
make PLATFORM=miyoomini build
```

Output goes to `build/` directory. Copy to SD card and test on device.

## Contributing

### Before You Submit

1. Run all checks:
```bash
make test lint format
```

2. Test on at least one platform if possible

3. Update documentation:
   - Platform README if hardware-specific
   - CLAUDE.md if changing architecture
   - This file if changing workflows

### Commit Messages

Keep it simple:
```
Add battery percentage to status bar

Show battery % next to icon when available.
Fallback to icon-only on platforms without %.
```

First line is summary (imperative mood). Body explains why and what changed.

## Common Tasks

### Adding a New Libretro Core

1. Add core to `workspace/<platform>/cores/` as git submodule (cores are platform-specific)
2. Add build rules to `workspace/<platform>/cores/makefile`
3. Create emulator pak with launch script
4. Test on target hardware

Note: Some platforms share cores (e.g., my282 copies cores from rg35xx due to same CPU).

### Fixing a Bug

1. Write a failing test first (if possible)
2. Fix the bug
3. Verify test passes
4. Run `make lint` to check for issues

### Updating Assets

Assets live in `skeleton/SYSTEM/res/`:
- `assets@1x.png` - UI sprites for 1x scale (320x240)
- `assets@2x.png` - UI sprites for 2x scale (640x480)
- `assets@3x.png` - UI sprites for 3x scale (960x720)
- `assets@4x.png` - UI sprites for 4x scale (1280x960+)
- `BPreplayBold-unhinted.otf` - UI font

Edit with pixel editor. Keep sprites on grid defined in `workspace/all/common/api.c`.

## Troubleshooting

### Docker Won't Start

Check Docker Desktop is running. Restart if needed.

### Build Fails

Try cleaning:
```bash
make clean
```

Then rebuild:
```bash
make PLATFORM=miyoomini
```

### Tests Fail

Tests run in Docker. Check test output carefully. Tests are strict about memory and undefined behavior.

Common issues:
- Buffer too small
- Uninitialized variables
- Off-by-one errors

### Platform Won't Boot

Check `workspace/<platform>/install/boot.sh` for errors. Boot scripts are platform-specific and finicky.

Enable verbose logging by editing boot script to redirect output:
```bash
./minui.elf > /tmp/minui.log 2>&1
```

Then check logs on device.

## Resources

- [Architecture Guide](ARCHITECTURE.md) - How MinUI works internally
- [Pak Development](PAKS.md) - Creating custom emulator paks
- [Platform READMEs](../workspace/) - Platform-specific docs
- [Main Project Docs](../CLAUDE.md) - Comprehensive reference

## Getting Help

- Check existing code for examples
- Platform READMEs document hardware quirks
- CLAUDE.md has detailed architecture notes
- GitHub issues for bugs and questions
