# MinUI Build System

This document explains how to build MinUI from source, including the build system architecture and toolchain setup.

## Overview

MinUI uses a Docker-based cross-compilation system that:
- Builds for 12+ ARM platforms from a macOS/Linux host
- Uses platform-specific toolchain containers
- Produces ready-to-deploy SD card images
- Creates release packages automatically

## Build System Architecture

```
┌─────────────────────────────────────────────┐
│         Host Machine (macOS/Linux)          │
│                                             │
│  ┌────────────────────────────────────┐    │
│  │     /makefile (Master Build)       │    │
│  │  - Setup skeleton                  │    │
│  │  - Invoke Docker for each platform │    │
│  │  - Package releases                │    │
│  └────────────────────────────────────┘    │
│                    ↓                        │
│  ┌────────────────────────────────────┐    │
│  │  /makefile.toolchain               │    │
│  │  - Docker container management     │    │
│  │  - Volume mounting                 │    │
│  │  - Build invocation                │    │
│  └────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│   Docker Container (per platform)           │
│                                             │
│  ┌────────────────────────────────────┐    │
│  │  /workspace/makefile               │    │
│  │  - Build all components            │    │
│  │  - Cross-compile for target        │    │
│  └────────────────────────────────────┘    │
│                    ↓                        │
│  ┌────────────────────────────────────┐    │
│  │  Component Makefiles               │    │
│  │  - Individual builds               │    │
│  │  - Link libraries                  │    │
│  └────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
                    ↓
         Compiled binaries → /build/
```

## Prerequisites

### Required Software

1. **Docker Desktop** - For cross-compilation containers
2. **Make** - GNU Make (usually pre-installed on macOS/Linux)
3. **Git** - For version control and toolchain fetching
4. **Basic Unix Tools** - cp, rm, zip, etc.

### Optional

- **zip** - For creating release archives
- **ImageMagick** - For generating asset variations (if modifying graphics)

## Quick Start

### Build for All Platforms

```bash
make
```

This runs the full build:
1. Sets up build directory
2. Compiles for all platforms
3. Creates release packages

### Build for Specific Platform

```bash
make PLATFORM=miyoomini
```

Supported platforms:
- `miyoomini` - Miyoo Mini/Plus
- `my282` - Miyoo Mini Flip
- `my355` - Miyoo Flip/A30
- `trimuismart` - Trimui Smart family
- `rg35xx` - RG35XX original
- `rg35xxplus` - RG35XX Plus family
- `rgb30` - RGB30
- `tg5040` - Trimui Smart Pro
- `m17` - M17
- `gkdpixel` - GKD Pixel/Mini
- `magicmini` - MagicX XU Mini M
- `zero28` - MagicX Mini Zero 28

### Interactive Shell (Development)

```bash
make PLATFORM=miyoomini shell
```

Opens interactive shell in Docker container for debugging and development.

## Build Phases

### Phase 1: Setup (`make setup`)

Creates build directory and prepares skeleton:

```bash
# Creates /build/ from /skeleton/
# Removes authoring files (.meta, .keep)
# Formats README files
# Generates build hash
```

**What happens:**
1. `/build/` directory created
2. `/skeleton/` contents copied to `/build/`
3. Placeholder files removed
4. README files processed (wrapped/formatted)
5. Git commit hash embedded in build

**Output:**
- `/build/SYSTEM/`
- `/build/BASE/`
- `/build/EXTRAS/`
- `/build/BOOT/`

### Phase 2: Platform Builds (`make [platform]`)

For each platform:

```bash
make PLATFORM=[platform] build
```

This invokes the Docker toolchain:

**What happens:**
1. Toolchain image pulled from GitHub (if needed)
2. Docker container started with workspace mounted
3. `/workspace/makefile` executed inside container
4. All components compiled for target platform
5. Binaries copied to `/build/SYSTEM/[platform]/`

**Build order** (inside container):
```
1. libmsettings.so
2. keymon.elf
3. minui.elf
4. minarch.elf
5. clock.elf, minput.elf, say.elf, syncsettings.elf
6. Platform-specific utilities
7. Libretro cores (optional, see cores build)
```

**Output** (per platform):
- `/build/SYSTEM/[platform]/bin/*.elf`
- `/build/SYSTEM/[platform]/lib/libmsettings.so`
- `/build/SYSTEM/[platform]/cores/*.so` (if cores built)

### Phase 3: Cores Build (Optional)

Cores can be built separately:

```bash
cd workspace/[platform]/cores
make
```

**What happens:**
1. Git clones libretro core repositories
2. Applies universal patches from `/workspace/all/cores/shared/`
3. Applies platform patches from `patches/`
4. Compiles core with platform-specific flags
5. Copies `.so` to `output/`

**Common cores:**
- fceumm (NES)
- gambatte (GB/GBC)
- gpsp (GBA)
- picodrive (Genesis/SMS/GG)
- snes9x2005_plus (SNES)
- pcsx_rearmed (PS1)

**Note:** Cores build can take hours. Pre-built cores available in releases.

### Phase 4: Special Platform Handling (`make special`)

Handles platform-specific packaging quirks:

**What happens:**
- Creates Miyoo family variants (miyoo354, miyoo355, etc.)
- Reorganizes boot files
- Applies platform-specific patches
- Creates symlinks where needed

**Output:**
- Additional platform directories in `/build/BOOT/`
- Modified system files for variants

### Phase 5: Packaging (`make package`)

Creates release archives:

```bash
cd build
zip -r ../releases/MinUI-[date]-[build]-base.zip SYSTEM/ BASE/ BOOT/
zip -r ../releases/MinUI-[date]-[build]-extras.zip EXTRAS/
zip -r ../releases/MinUI-[date]-[build].zip SYSTEM/
```

**Release files:**
- `MinUI-YYYYMMDD-N-base.zip` - Full base installation
- `MinUI-YYYYMMDD-N-extras.zip` - Extra cores and tools
- `MinUI-YYYYMMDD-N.zip` - Update package (SYSTEM only)
- `version.txt` - Version information
- `commits.txt` - Commit history

## Toolchain System

### Toolchain Images

MinUI uses pre-built Docker images containing:
- Cross-compilation toolchain (GCC for ARM)
- Platform-specific libraries (SDL, etc.)
- Build tools (make, pkg-config, etc.)
- Development headers

**Toolchain sources:**
```
https://github.com/shauninman/union-[platform]-toolchain/
```

**Available toolchains:**
- union-miyoomini-toolchain
- union-rg35xx-toolchain
- union-rg35xxplus-toolchain
- union-trimuismart-toolchain
- etc.

### Toolchain Operations

**Pull toolchain image:**
```bash
make PLATFORM=miyoomini toolchain
```

**Interactive shell:**
```bash
make PLATFORM=miyoomini shell
```

**Non-interactive build:**
```bash
make PLATFORM=miyoomini build
```

### Inside the Container

**Mounted volumes:**
- Host: `/Users/[user]/[repo]/workspace/`
- Guest: `/root/workspace/`

**Environment variables:**
- `UNION_PLATFORM=[platform]` - Current platform
- `CROSS_COMPILE=[prefix]` - Toolchain prefix
- `CC, CXX, LD, AR, STRIP` - Compiler tools

**Available tools:**
- `arm-linux-gnueabihf-gcc` (or similar)
- `make`
- `pkg-config`
- Platform-specific libraries

## Workspace Build Process

### `/workspace/makefile`

Master makefile for workspace builds.

**Targets:**
```makefile
all: [platform]           # Build everything for platform
[platform]: deps bin cores  # Platform-specific build
deps: libmsettings        # Dependencies
bin: minui minarch utils  # Main binaries
cores: platform-cores     # Libretro cores (if enabled)
clean: clean-all          # Remove build artifacts
```

**Build sequence:**
```bash
make libmsettings         # Settings library
make keymon               # Input monitor
make minui                # Launcher
make minarch              # Frontend
make clock                # Clock utility
make minput               # Input tester
make say                  # Notification
make syncsettings         # Settings sync
make platform-utils       # Platform-specific tools
make cores                # Cores (optional, slow)
```

### Component Makefiles

Each component has a standardized makefile:

**Example** (`/workspace/all/minui/makefile`):
```makefile
PLATFORM = $(UNION_PLATFORM)

ifeq ($(PLATFORM),miyoomini)
    CROSS_COMPILE = /opt/miyoomini-toolchain/bin/arm-linux-gnueabihf-
endif

CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

TARGET = minui.elf
SOURCE = minui.c ../common/api.c ../common/utils.c ../../$(PLATFORM)/platform/platform.c

CFLAGS = -Ofast -std=gnu99 -DPLATFORM=\"$(PLATFORM)\"
LDFLAGS = -lmsettings -lSDL -lSDL_image -lSDL_ttf -lpthread -lm -lz

all: $(TARGET)

$(TARGET): $(SOURCE)
    $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
    $(STRIP) $@

clean:
    rm -f $(TARGET)
```

**Key points:**
- Platform detected from `UNION_PLATFORM` env var
- Cross-compiler selected based on platform
- All source files compiled in single invocation
- Output stripped to reduce size
- Links against platform libraries

### Cores Makefile

**Template pattern** (`/workspace/all/cores/makefile`):

```makefile
# Download core
$(SRC_DIR)/[core]:
    git clone [repo-url] $@
    cd $@ && git checkout [commit-hash]

# Apply patches
$(SRC_DIR)/[core]/.patched: $(SRC_DIR)/[core]
    # Apply universal patches
    -patch -d $(SRC_DIR)/[core] -p1 < ../shared/generic/[core].patch
    # Apply platform patches
    -patch -d $(SRC_DIR)/[core] -p1 < patches/[core].patch
    touch $@

# Build core
$(OUTPUT_DIR)/[core]_libretro.so: $(SRC_DIR)/[core]/.patched
    $(MAKE) -C $(SRC_DIR)/[core] -f Makefile platform=$(PLATFORM)
    cp $(SRC_DIR)/[core]/[core]_libretro.so $@
```

**Customization per core:**
- Custom git repository
- Specific commit hash
- Custom makefile location
- Build flags
- Output path

## Build Configuration

### Global Configuration

**Platform selection** (in root `/makefile`):
```makefile
PLATFORMS = miyoomini trimuismart rg35xx rg35xxplus my355 \
            tg5040 zero28 rgb30 m17 gkdpixel my282 magicmini
```

**Toolchain mapping** (in `/makefile.toolchain`):
```makefile
DOCKER_IMAGE_miyoomini = ghcr.io/shauninman/union-miyoomini-toolchain:latest
DOCKER_IMAGE_rg35xx = ghcr.io/shauninman/union-rg35xx-toolchain:latest
# etc.
```

### Component Configuration

**Compiler flags** (common):
```makefile
CFLAGS = -Ofast              # Maximum optimization
         -std=gnu99          # C99 with GNU extensions
         -DPLATFORM="..."    # Platform identifier
         -Wall               # All warnings
         -Wno-unused         # Suppress unused warnings
```

**Linker flags** (common):
```makefile
LDFLAGS = -lmsettings        # Platform settings
          -lSDL              # Simple DirectMedia Layer
          -lSDL_image        # Image loading
          -lSDL_ttf          # Font rendering
          -lpthread          # Threading
          -lm                # Math
          -lz                # Compression
          -ldl               # Dynamic loading
```

### Platform-Specific Flags

Different platforms may have additional flags:

**Miyoo Mini:**
```makefile
CFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4
```

**RG35XX:**
```makefile
CFLAGS += -march=armv8-a
```

## Incremental Builds

### Build Only Changed Components

```bash
# After modifying minui.c:
cd workspace/all/minui
make PLATFORM=miyoomini

# Copy to build:
cp minui.elf /path/to/build/SYSTEM/miyoomini/bin/
```

### Rebuild Single Component

```bash
make -C workspace/all/minui clean
make -C workspace/all/minui PLATFORM=miyoomini
```

### Skip Cores Build

Cores take hours to compile. To skip:

```bash
# Modify workspace/[platform]/cores/makefile
# Comment out core definitions

# Or use pre-built cores from release
```

## Debugging Build Issues

### Enable Verbose Output

```bash
make VERBOSE=1
```

### Check Toolchain

```bash
make PLATFORM=miyoomini shell
# Inside container:
which arm-linux-gnueabihf-gcc
arm-linux-gnueabihf-gcc --version
```

### Manual Compilation

```bash
make PLATFORM=miyoomini shell
cd /root/workspace/all/minui
arm-linux-gnueabihf-gcc -v minui.c
```

### Common Issues

**1. Docker not running:**
```
Error: Cannot connect to Docker daemon
Solution: Start Docker Desktop
```

**2. Toolchain image not found:**
```
Error: Unable to find image 'ghcr.io/...'
Solution: Check internet connection, verify image exists
```

**3. Missing libraries:**
```
Error: undefined reference to 'SDL_Init'
Solution: Check toolchain has SDL installed
```

**4. Permission denied:**
```
Error: Permission denied writing to /build/
Solution: Check directory permissions, run with appropriate user
```

## Build Performance

### Build Times (Approximate)

| Task | Duration |
|------|----------|
| Setup | 5-10 seconds |
| Single platform (no cores) | 1-2 minutes |
| Single platform (with cores) | 1-3 hours |
| All platforms (no cores) | 15-30 minutes |
| All platforms (with cores) | 12-36 hours |

### Optimization Tips

1. **Use parallel builds:**
   ```bash
   make -j$(nproc)
   ```

2. **Skip cores:**
   - Use pre-built cores from releases
   - Only build cores when needed

3. **Build specific platforms:**
   ```bash
   make PLATFORM=miyoomini
   ```

4. **Incremental builds:**
   - Only rebuild changed components
   - Keep build directory between builds

5. **Use local Docker images:**
   - Pull toolchains once, reuse

## Output Structure

### Build Directory

After successful build:

```
build/
├── SYSTEM/
│   ├── res/                      # Shared resources
│   └── [platform]/               # Per-platform
│       ├── bin/                  # Executables
│       │   ├── minui.elf
│       │   ├── minarch.elf
│       │   ├── keymon.elf
│       │   └── ...
│       ├── lib/
│       │   └── libmsettings.so
│       ├── cores/                # Cores (if built)
│       │   ├── fceumm_libretro.so
│       │   └── ...
│       └── paks/                 # System paks
│           └── Emus/
├── BASE/                         # User files
├── EXTRAS/                       # Optional content
└── BOOT/                         # Bootstrap
```

### Release Packages

```
releases/
├── MinUI-20250107-1-base.zip
├── MinUI-20250107-1-extras.zip
├── MinUI-20250107-1.zip
├── version.txt
└── commits.txt
```

## Clean Targets

```bash
make clean              # Remove build artifacts
make clean-all          # Remove build/ and releases/
make clean-workspace    # Clean workspace builds
make clean-cores        # Remove downloaded cores
```

## Advanced: Building Without Docker

For native compilation (if on ARM Linux):

```bash
cd workspace
export UNION_PLATFORM=miyoomini
make
```

**Requirements:**
- ARM Linux system
- Development libraries installed
- Matching architecture to target platform

**Not recommended** - Docker ensures reproducible builds.

## CI/CD Integration

MinUI can be built in GitHub Actions or similar:

```yaml
- name: Build MinUI
  run: |
    make setup
    make PLATFORM=miyoomini
    make package
```

**Considerations:**
- Docker must be available in CI environment
- Large Docker images (1-2 GB per toolchain)
- Long build times for cores
- Artifact storage for releases

## Next Steps

- [Development Guide](development-guide.md) - Contributing to MinUI
- [Adding Platform Support](adding-platform.md) - Porting to new devices
- [Components Guide](components.md) - Understanding the codebase
