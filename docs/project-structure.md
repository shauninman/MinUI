# MinUI Project Structure

This document describes the directory layout and organization of the MinUI repository.

## Repository Root

```
/
├── workspace/         # Core source code
├── skeleton/          # SD card template structure
├── github/            # Documentation assets
├── releases/          # Built release packages (generated)
├── build/             # Build output (generated)
├── docs/              # Technical documentation
├── makefile           # Master build orchestration
├── makefile.toolchain # Docker toolchain setup
├── README.md          # User-facing documentation
├── PAKS.md            # Pak creation guide
├── todo.txt           # Development notes
├── commits.sh         # Release history script
└── .gitignore         # Git configuration
```

## Workspace Directory

The workspace contains all source code and is organized by platform and component.

### `/workspace/` Structure

```
workspace/
├── all/                      # Platform-independent code
│   ├── minui/                # Main launcher
│   ├── minarch/              # Libretro frontend
│   ├── common/               # Shared libraries
│   ├── cores/                # Core build system
│   ├── clock/                # Clock utility
│   ├── minput/               # Input testing tool
│   ├── say/                  # Notification utility
│   ├── syncsettings/         # Settings sync daemon
│   └── readmes/              # README utilities
│
├── miyoomini/                # Miyoo Mini platform
├── my282/                    # Miyoo Mini Flip
├── my355/                    # Miyoo Flip/A30
├── trimuismart/              # Trimui Smart family
├── rg35xx/                   # RG35XX original
├── rg35xxplus/               # RG35XX Plus family
├── rgb30/                    # RGB30
├── tg5040/                   # Trimui Smart Pro
├── m17/                      # M17
├── gkdpixel/                 # GKD Pixel/Mini
├── magicmini/                # MagicX XU Mini M
├── zero28/                   # MagicX Mini Zero 28
├── macos/                    # macOS testing
│
└── makefile                  # Workspace build orchestrator
```

### Platform-Independent Code (`/workspace/all/`)

#### MinUI Launcher (`/workspace/all/minui/`)
```
minui/
├── minui.c            # Main launcher (1,704 lines)
├── makefile           # Build configuration
└── credits.txt        # Attribution
```

**Purpose**: Main UI for ROM browsing and launching

**Key Files**:
- `minui.c` - Entry point, menu system, ROM scanning, pak launching

#### Minarch Frontend (`/workspace/all/minarch/`)
```
minarch/
├── minarch.c          # Libretro frontend (4,830 lines)
├── makefile           # Build configuration
└── credits.txt        # Attribution
```

**Purpose**: Runs libretro emulator cores

**Key Files**:
- `minarch.c` - Core loading, in-game menu, save states, video/audio

#### Common Libraries (`/workspace/all/common/`)
```
common/
├── api.c              # Common API implementation (1,722 lines)
├── api.h              # API header (347 lines)
├── scaler.c           # Video scaling (110 KB)
├── scaler.h           # Scaler header (22 KB)
├── utils.c            # Utility functions (5 KB)
├── utils.h            # Utility header (821 bytes)
├── defines.h          # Global constants (5 KB)
└── sdl.h              # SDL version abstraction (1 KB)
```

**Purpose**: Shared functionality across all applications

**Key APIs**:
- **GFX_*** - Graphics rendering, asset loading, text display
- **SND_*** - Audio initialization and buffering
- **PAD_*** - Input handling with auto-repeat
- **PWR_*** - Power management, sleep, battery
- **LID_*** - Lid state detection

#### Cores Build System (`/workspace/all/cores/`)
```
cores/
├── makefile           # Template-based core builder
├── shared/            # Shared core patches
│   └── generic/       # Universal patches
└── README.txt         # Core building documentation
```

**Purpose**: Downloads, patches, and builds libretro cores

#### Utility Applications

**Clock** (`/workspace/all/clock/`):
```
clock/
├── clock.c            # Simple clock display
└── makefile
```

**Minput** (`/workspace/all/minput/`):
```
minput/
├── minput.c           # Input testing/configuration
└── makefile
```

**Say** (`/workspace/all/say/`):
```
say/
├── say.c              # Notification display
└── makefile
```

**Syncsettings** (`/workspace/all/syncsettings/`):
```
syncsettings/
├── syncsettings.c     # Settings restoration daemon
└── makefile
```

**Readmes** (`/workspace/all/readmes/`):
```
readmes/
└── formatreadmes.c    # README formatting utility
```

### Platform-Specific Code

Each platform directory follows a similar structure:

```
[platform]/
├── platform/          # Platform abstraction layer
│   ├── platform.c     # Platform implementation
│   └── platform.h     # Platform constants
│
├── libmsettings/      # Settings library
│   ├── msettings.c    # Settings implementation
│   └── msettings.h    # Settings API
│
├── keymon/            # Input monitor daemon
│   ├── keymon.c       # Key monitoring
│   └── makefile
│
├── cores/             # Platform cores configuration
│   ├── makefile       # Which cores to build
│   ├── patches/       # Platform-specific patches
│   ├── output/        # Built .so files (generated)
│   └── src/           # Downloaded sources (generated)
│
└── [platform-specific utilities]
```

#### Platform.h/platform.c

**platform.h** defines:
- Button mappings (SDL keys, raw codes, joystick IDs)
- Screen dimensions
- System paths
- Platform capabilities
- Hardware-specific constants

**platform.c** implements:
- `PLAT_initInput()` - Input device setup
- `PLAT_quitInput()` - Input cleanup
- `PLAT_pollInput()` - Read button states
- `PLAT_initVideo()` - Display initialization
- `PLAT_quitVideo()` - Display cleanup
- `PLAT_clearVideo()` - Screen clearing
- `PLAT_clearAll()` - Full screen clear
- `PLAT_setVsync()` - V-sync control
- `PLAT_vsync()` - Wait for v-sync
- `PLAT_flip()` - Present frame
- `PLAT_scale()` - Software scaling
- `PLAT_getBatteryStatus()` - Battery level
- `PLAT_enableBacklight()` - Display on/off
- `PLAT_powerOff()` - System shutdown
- `PLAT_setCPUSpeed()` - CPU frequency

#### libmsettings

**msettings.h** API:
- `InitSettings()` - Load settings
- `QuitSettings()` - Save settings
- `GetBrightness()` / `SetBrightness()` - Backlight control
- `GetVolume()` / `SetVolume()` - Audio control
- `GetLastPlay()` / `SetLastPlay()` - Resume tracking

**msettings.c** implementation:
- Platform-specific hardware access
- `/sys/class/backlight/` for brightness
- ALSA or platform mixer for volume
- Settings file persistence

#### keymon

**keymon.c**:
- Opens raw input devices (`/dev/input/eventX`)
- Monitors for button presses
- Handles volume up/down shortcuts
- Handles brightness shortcuts
- Implements sleep timer
- Prevents system from interfering with MinUI

#### Cores Configuration

**cores/makefile**:
- Lists which cores to build
- Specifies core sources (git repos)
- Defines patches to apply
- Sets build flags

Example platforms build these stock cores:
- fceumm (NES)
- gambatte (GB/GBC)
- gpsp (GBA)
- picodrive (Genesis/SMS/GG/SegaCD)
- snes9x2005_plus (SNES)
- pcsx_rearmed (PS1)

#### Platform-Specific Utilities

**Miyoo Mini** (`/workspace/miyoomini/`):
- `show/` - Splash screen display
- `overclock/` - CPU frequency control
- `batmon/` - Battery monitor
- `lumon/` - Luminance control
- `blank/` - Screen blanking

**MY282** (`/workspace/my282/`):
- `libmstick/` - Analog stick library
- `show/` - Splash screen

**RG35XX Plus** (`/workspace/rg35xxplus/`):
- `init/` - System initialization
- `show/` - Splash screen

**Zero28** (`/workspace/zero28/`):
- `bl/` - Backlight control
- `show/` - Splash screen

## Skeleton Directory

The skeleton defines the SD card structure that gets populated during the build.

### `/skeleton/` Structure

```
skeleton/
├── SYSTEM/            # System files
│   ├── res/           # Shared resources
│   └── [platform]/    # Platform binaries (populated at build)
│
├── BASE/              # User-facing structure
│   ├── Roms/          # ROM directories
│   ├── Saves/         # Save files
│   ├── Bios/          # BIOS files
│   ├── README.txt     # User guide
│   ├── miyoo/         # Miyoo boot files
│   ├── trimui/        # Trimui boot files
│   └── magicx/        # MagicX boot files
│
├── EXTRAS/            # Optional content
│   ├── Emus/          # Extra emulator paks
│   ├── Tools/         # Utility paks
│   ├── Roms/          # Extra console folders
│   ├── Saves/         # Extra saves folders
│   ├── Bios/          # Extra BIOS folders
│   └── README.txt     # Extras guide
│
└── BOOT/              # Bootstrap files
    ├── common/        # Shared updater
    ├── miyoo/         # Miyoo family bootstrap
    ├── trimui/        # Trimui family bootstrap
    └── magicx/        # MagicX family bootstrap
```

### System Resources (`/skeleton/SYSTEM/res/`)

```
SYSTEM/res/
├── BPreplayBold-unhinted.otf    # Main UI font
├── assets@1x.png                # UI sprites (240p)
├── assets@2x.png                # UI sprites (480p)
├── assets@3x.png                # UI sprites (720p)
├── assets@4x.png                # UI sprites (960p+)
├── grid-1x.png                  # CRT grid overlay
├── grid-2x.png
├── grid-3x.png
├── grid-4x.png
├── line-1x.png                  # Scanline overlay
├── line-2x.png
├── line-3x.png
├── line-4x.png
└── charging-*.png               # Charging screen graphics
```

### Platform Binaries (`/skeleton/SYSTEM/[platform]/`)

Populated during build from workspace:

```
SYSTEM/[platform]/
├── bin/
│   ├── minui.elf              # Main launcher
│   ├── minarch.elf            # Libretro frontend
│   ├── keymon.elf             # Input monitor
│   ├── syncsettings.elf       # Settings sync
│   ├── say.elf                # Notifications
│   ├── clock.elf              # Clock utility
│   ├── install.sh             # Update script
│   └── [platform utilities]   # Platform-specific tools
│
├── lib/
│   └── libmsettings.so        # Settings library
│
├── cores/
│   ├── fceumm_libretro.so     # NES core
│   ├── gambatte_libretro.so   # GB/GBC core
│   ├── gpsp_libretro.so       # GBA core
│   ├── picodrive_libretro.so  # Genesis/SMS core
│   ├── snes9x2005_plus_libretro.so  # SNES core
│   └── pcsx_rearmed_libretro.so     # PS1 core
│
└── paks/
    ├── MinUI.pak/             # System updater
    └── Emus/                  # Emulator paks
        ├── GB.pak/
        ├── GBC.pak/
        ├── GBA.pak/
        ├── FC.pak/
        ├── SFC.pak/
        ├── MD.pak/
        ├── PS.pak/
        └── PAK.pak/
```

### ROM Directories (`/skeleton/BASE/Roms/`)

```
Roms/
├── Game Boy (GB)/
├── Game Boy Color (GBC)/
├── Game Boy Advance (GBA)/
├── Nintendo Entertainment System (FC)/
├── Super Nintendo Entertainment System (SFC)/
├── Sega Genesis (MD)/
├── Sony PlayStation (PS)/
└── Native Games (PAK)/
```

Directory names follow pattern: `Console Name (TAG)`
- Display name shown in UI
- Tag used to identify emulator pak

### Extras Structure (`/skeleton/EXTRAS/`)

```
EXTRAS/
├── Emus/[platform]/
│   ├── MGBA.pak/              # Better GBA emulation
│   ├── SGB.pak/               # Super Game Boy
│   ├── PKM.pak/               # Pokemon Mini
│   ├── NGP.pak/               # Neo Geo Pocket
│   ├── NGPC.pak/              # Neo Geo Pocket Color
│   ├── PCE.pak/               # TurboGrafx-16
│   ├── P8.pak/                # PICO-8
│   ├── SUPA.pak/              # Better SNES emulation
│   ├── VB.pak/                # Virtual Boy
│   ├── GG.pak/                # Game Gear
│   └── SMS.pak/               # Sega Master System
│
└── Tools/[platform]/
    ├── Clock.pak/             # Clock display
    ├── Input.pak/             # Input tester
    ├── Files.pak/             # File browser
    ├── ADBD.pak/              # ADB daemon (Miyoo Mini)
    ├── IP.pak/                # IP display (Miyoo Mini)
    ├── Bootlogo.pak/          # Boot logo changer
    └── [platform-specific]/
```

## Build Output

### `/build/` Directory (Generated)

Created during build process, mirrors skeleton with populated binaries:

```
build/
├── SYSTEM/
│   ├── res/                   # Copied from skeleton
│   └── [platform]/            # Populated with compiled files
├── BASE/                      # Copied from skeleton
├── EXTRAS/                    # Copied from skeleton
└── BOOT/                      # Copied from skeleton
```

### `/releases/` Directory (Generated)

Contains final release packages:

```
releases/
├── MinUI-YYYYMMDD-N-base.zip      # Base release
├── MinUI-YYYYMMDD-N-extras.zip    # Extras package
├── MinUI-YYYYMMDD-N.zip           # Update package
├── version.txt                     # Version info
└── commits.txt                     # Commit history
```

## Documentation Directory

### `/docs/` Structure

```
docs/
├── README.md                  # Documentation index
├── architecture.md            # System design
├── project-structure.md       # This file
├── components.md              # Component details
├── build-system.md            # Build process
├── platform-abstraction.md    # Platform HAL guide
├── pak-system.md              # Pak architecture
├── development-guide.md       # Developer guide
├── adding-platform.md         # Platform porting
├── api-reference.md           # API documentation
├── platforms.md               # Platform details
├── file-formats.md            # Config file specs
└── troubleshooting.md         # Common issues
```

## Key File Locations Quick Reference

| Purpose | Location |
|---------|----------|
| Main launcher | `/workspace/all/minui/minui.c` |
| Libretro frontend | `/workspace/all/minarch/minarch.c` |
| Common API | `/workspace/all/common/api.c` |
| Video scaler | `/workspace/all/common/scaler.c` |
| Platform HAL | `/workspace/[platform]/platform/platform.c` |
| Settings library | `/workspace/[platform]/libmsettings/msettings.c` |
| Input monitor | `/workspace/[platform]/keymon/keymon.c` |
| Master makefile | `/makefile` |
| Toolchain setup | `/makefile.toolchain` |
| User guide | `/skeleton/BASE/README.txt` |
| Pak guide | `/PAKS.md` |
| UI font | `/skeleton/SYSTEM/res/BPreplayBold-unhinted.otf` |
| UI graphics | `/skeleton/SYSTEM/res/assets@*.png` |

## Build Artifacts

Files and directories created during build (in `.gitignore`):

- `/build/` - Build output directory
- `/releases/` - Release packages
- `/workspace/*/cores/src/` - Downloaded core sources
- `/workspace/*/cores/output/` - Compiled cores
- `*.elf` - Executable files
- `*.so` - Shared libraries
- `*.o` - Object files
- `.tmp_update/` - Update system temporary files

## Version Control

The repository tracks:
- All source code
- Skeleton structure (with `.keep` files for empty dirs)
- Documentation
- Build scripts
- Resource files (fonts, graphics)

The repository ignores:
- Build artifacts
- Compiled binaries
- Downloaded dependencies
- Release packages
- Platform-specific temporary files
