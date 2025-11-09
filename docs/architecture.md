# MinUI Architecture

This document explains how MinUI is structured and how the pieces fit together.

## Core Concept

MinUI uses a **platform abstraction layer** to run the same code on 20+ different handheld devices. Write once, compile for each platform with hardware-specific constants.

## The Three Layers

### 1. Common Code (`workspace/all/`)

Platform-independent C code that works everywhere:

- **minui** (`minui.c`) - The launcher
  - Browse ROMs by folder
  - Track recently played games
  - Launch emulator paks
  - Display hardware status (battery, volume, brightness)

- **minarch** (`minarch.c`) - The libretro frontend
  - Load and run emulator cores
  - Save state management (auto-save to slot 9)
  - In-game menu (states, disc changing, options)
  - Video scaling and audio mixing

- **common utilities** (`common/`)
  - `utils.c` - String helpers, file I/O, ROM name cleanup
  - `api.c` - Graphics (GFX_*), Audio (SND_*), Input (PAD_*), Power (PWR_*)
  - `scaler.c` - Optimized pixel scaling (NEON when available)

### 2. Platform Definitions (`workspace/<platform>/platform/`)

Each device defines hardware constants in `platform.h`:

```c
#define PLATFORM "miyoomini"
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define FIXED_SCALE 2
#define SDCARD_PATH "/mnt/SDCARD"
#define BUTTON_A SDLK_SPACE
#define BUTTON_B SDLK_LCTRL
```

The common code uses these to adapt to each device. One codebase, multiple targets.

Some platforms also have `platform.c` for complex hardware-specific implementations (video initialization, HDMI switching, etc.).

### 3. Platform Components (`workspace/<platform>/`)

Device-specific daemons and utilities:

- **keymon** - Button monitoring daemon
  - Monitors hardware buttons (volume, power, etc.)
  - Updates settings (brightness, volume)
  - Handles sleep/wake, shutdown

- **libmsettings** - Settings library
  - Persists volume, brightness, etc.
  - Shared between minui, minarch, and tools
  - Uses shared memory or files for IPC

- **Other components** (platform-specific):
  - `batmon` - Battery overlay
  - `lumon` - Backlight control
  - `overclock` - CPU frequency adjustment
  - `show` - Boot splash display

## How It Works

### Starting Up

1. Device boots → runs platform boot script (`workspace/<platform>/install/boot.sh`)
2. Boot script displays splash screen (installing/updating if needed)
3. Launches MinUI via `.system/<platform>/paks/MinUI.pak/launch.sh`
4. MinUI reads ROM folders and displays launcher

### Launching a Game

1. User selects ROM in launcher
2. MinUI calls the appropriate pak's `launch.sh` script
3. Pak script runs `minarch.elf <core> <rom>`
4. Minarch loads the libretro core and starts emulation
5. User presses MENU → in-game menu appears
6. User saves state, changes settings, etc.
7. User quits → returns to launcher
8. Next launch auto-resumes from slot 9

### Platform Abstraction

Common code calls abstract APIs:
```c
GFX_init();                    // Initialize display
PAD_poll();                    // Read button state
PWR_getBatteryLevel();         // Get battery %
```

Each platform implements these differently:
- **miyoomini**: SDL keyboard + custom battery ADC
- **rgb30**: SDL2 joystick + sysfs battery
- **tg5040**: SDL2 joystick + inverted ALSA volume

The abstraction hides complexity. Common code doesn't care how buttons work.

## Directory Layout

### Source Code
```
workspace/
├── all/                   # Runs everywhere
│   ├── minui/            # Launcher
│   ├── minarch/          # Emulator frontend
│   └── common/           # Shared API
│
└── miyoomini/            # Example platform
    ├── platform/         # Hardware definitions
    ├── keymon/           # Button daemon
    ├── libmsettings/     # Settings library
    ├── install/          # Boot script + splash screens
    └── cores/            # Libretro cores (git submodules)
```

### Installation Files
```
skeleton/
├── SYSTEM/
│   └── res/              # Shared assets
│       ├── assets@2x.png # UI sprite sheet
│       └── BPreplayBold-unhinted.otf
│
├── BOOT/                 # Boot scripts
└── EXTRAS/               # Optional paks
```

### Output
```
build/
├── SYSTEM/               # Core system files
├── BOOT/                 # Bootloaders
└── EXTRAS/               # Optional components
```

## The Pak System

MinUI is extended through "paks" - folders ending in `.pak` with a `launch.sh` script inside.

### Emulator Paks

Live in `Emus/<platform>/`:
```
Emus/miyoomini/
└── GB.pak/
    ├── launch.sh          # Launches core for this system
    └── default.cfg        # Default core settings
```

The launcher maps ROM folders to paks by tag:
```
Roms/Game Boy (GB)/     →    Emus/miyoomini/GB.pak/
```

### Tool Paks

Live in `Tools/<platform>/`:
```
Tools/miyoomini/
└── Files.pak/
    ├── launch.sh          # Launches file manager
    └── res/               # Tool assets
```

Tools appear in the launcher as a separate category.

See [PAKS.md](PAKS.md) for complete pak development guide.

## Multi-Resolution Support

MinUI supports devices from 320x240 to 1280x720 using a scale factor:

- **1x**: 320x240 devices (trimuismart, gkdpixel)
- **2x**: 640x480 devices (most platforms)
- **3x**: 960x720 devices (tg5040 brick)
- **4x**: 1280x960+ devices (future)

Each platform defines `FIXED_SCALE` in `platform.h`. At startup, MinUI loads the appropriate sprite sheet:

```c
sprintf(asset_path, RES_PATH "/assets@%ix.png", FIXED_SCALE);
```

All UI coordinates use `SCALE1()`, `SCALE2()`, etc. macros:
```c
SDL_Rect button = {SCALE4(10, 20, 30, 40)};  // Scales all 4 values
```

This way UI code is resolution-independent.

## Input Handling

MinUI supports three input methods (platforms use one or more):

1. **SDL Keyboard**: `BUTTON_A = SDLK_SPACE`
2. **SDL Joystick**: `JOY_A = 0`
3. **Evdev Codes**: `CODE_A = 57`

Platforms define which buttons use which method. Some use multiple (e.g., miyoomini uses SDL keyboard for games, evdev codes for keymon).

Common code reads buttons through the PAD API:
```c
PAD_poll();
if (PAD_justPressed(BTN_A)) { /* ... */ }
```

The platform layer handles the actual hardware polling.

## Graphics Pipeline

### Display Initialization

1. Platform opens framebuffer or SDL window
2. Creates surfaces for screen and layers
3. Sets up pixel format (usually RGB565)

### Rendering

MinUI uses double-buffering:
```c
GFX_clear(screen);              // Clear back buffer
GFX_blitText(...);              // Draw UI elements
GFX_flip(screen);               // Show back buffer
```

### Asset System

UI sprites live in a single sprite sheet (`assets@Nx.png`). Each sprite has a defined rectangle in `api.c`:

```c
asset_rects[ASSET_BATTERY] = (SDL_Rect){SCALE4(47, 51, 17, 10)};
```

Draw sprites with:
```c
GFX_blitAsset(ASSET_BATTERY, screen, x, y, width, height, rotation);
```

## Save States

MinUI has 9 save state slots per game:
- **Slots 0-8**: Manual saves (accessible in-game menu)
- **Slot 9**: Auto-save (created on quit, loaded on resume)

State files live in `.userdata/shared/<TAG>-<core-name>/`:
```
.userdata/shared/GB-gambatte/
├── Pokemon.st0      # Manual save slot 0
├── Pokemon.st9      # Auto-save slot 9
└── Pokemon.srm      # Save RAM (battery saves)
```

Save states are shared across all platforms (unlike per-game configs which are platform-specific). Press X in launcher to resume from last manual state instead of auto-state.

## Memory Management

### Stack vs Heap

MinUI prefers stack allocation for speed:
```c
char path[MAX_PATH];  // 512 bytes on stack
```

Use heap only when necessary:
```c
char* data = allocFile(path);  // Returns malloc'd memory
// ... use data ...
free(data);  // Caller must free
```

### SDL Surfaces

SDL surfaces are reference counted:
```c
SDL_Surface* img = IMG_Load(path);
// ... use img ...
SDL_FreeSurface(img);  // Always free
```

## Configuration Files

### Emulator Options

Each pak can have a `default.cfg`:
```
upscaler = 0
aspect = fill
filter = nearest

bind Up = UP
bind Down = DOWN
bind A Button = A
```

Users can override per-game in `.userdata/<platform>/<tag>/<rom-name>/`.

### Recent Games

`/.minui/recent.txt` tracks recently played games:
```
/path/to/rom.gb
/path/to/rom.nes
```

Launcher shows these first for quick access.

## Boot Process

1. Device hardware boots
2. Runs boot script from device-specific location
3. Boot script:
   - Checks for `MinUI.zip` (update)
   - Displays splash screen if updating
   - Extracts update or launches MinUI
4. MinUI launcher starts
5. User navigates and plays games
6. On quit, device reboots or powers off (prevents stock firmware access)

## Performance Optimizations

### NEON SIMD

Platforms with `HAS_NEON` can use ARM SIMD instructions:
```c
#ifdef HAS_NEON
    scale_neon(src, dst, width, height);
#else
    scale_c(src, dst, width, height);
#endif
```

Used in `scaler.c` for fast pixel scaling.

### Frame Pacing

Minarch maintains 60fps by:
- Locking to vsync when possible
- Throttling with `SDL_Delay()` when vsync unavailable
- Skipping frames if too slow (rare)

### Memory Efficiency

- Reuse surfaces where possible
- Free immediately after use
- Keep texture cache small
- Avoid allocations in hot paths

## Thread Safety

MinUI is mostly single-threaded except:
- Some platforms use background threads for HDMI monitoring
- Keymon runs as separate process
- Settings use shared memory or files for IPC

No complex locking needed.

## Platform-Specific Code

When you need platform-specific behavior, use `#ifdef`:

```c
#ifdef PLATFORM_MIYOOMINI
    // Special code for Miyoo Mini
#endif
```

Or add to `platform.c` and call from common code:
```c
// In platform.c
void PLAT_initSpecialHardware(void) {
    // Platform-specific initialization
}

// In common code
#ifdef HAS_SPECIAL_HARDWARE
    PLAT_initSpecialHardware();
#endif
```

## File Paths

All paths use forward slashes. Platforms define base paths:
```c
#define SDCARD_PATH "/mnt/SDCARD"
#define ROMS_PATH SDCARD_PATH "/Roms"
#define SYSTEM_PATH SDCARD_PATH "/.system/" PLATFORM
```

Common code uses these macros. Never hardcode paths.

## Resources

- [Development Guide](DEVELOPMENT.md) - Building and testing
- [Pak Development](PAKS.md) - Creating custom paks
- [Platform READMEs](../workspace/) - Hardware-specific details
- [Project Docs](../CLAUDE.md) - Comprehensive technical reference
