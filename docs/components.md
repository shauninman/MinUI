# MinUI Components Guide

This document provides detailed information about each component in the MinUI system.

## Core Applications

### MinUI Launcher

**Location**: `/workspace/all/minui/minui.c` (1,704 lines)

**Purpose**: Main user interface for browsing and launching ROMs.

#### Responsibilities

1. **ROM Scanning**
   - Recursively scans `/Roms/` directories
   - Identifies console type from directory tags (e.g., "(GB)", "(GBA)")
   - Filters for supported file extensions
   - Generates display names by hiding extensions and tags

2. **Collections Management**
   - Reads `.txt` files in collections directories
   - Displays custom ROM lists
   - Supports manual ROM curation

3. **Recent Games Tracking**
   - Maintains list of last 10 played games
   - Auto-resume: returns to last game on boot
   - Quick access to frequently played titles

4. **User Interface**
   - Pill-style navigation
   - Console icons
   - Battery/charging indicator
   - Volume and brightness display
   - Clean, minimal aesthetic

5. **Pak Launching**
   - Identifies appropriate `.pak` directory for console
   - Executes `launch.sh` with ROM path
   - Handles return from emulators

#### Key Functions

```c
main()                    // Entry point, main loop
scanDirectory()           // Recursive ROM scanning
getDisplayName()          // Smart name formatting
launchGame()              // Execute pak launch script
drawUI()                  // Render interface
handleInput()             // Process button presses
```

#### Configuration

MinUI reads settings from:
- `/mnt/SDCARD/.minui/last.txt` - Last played ROM
- `/mnt/SDCARD/.minui/recent.txt` - Recent games list
- Platform settings via libmsettings

#### ROM Name Processing

MinUI automatically formats ROM names:
- Hides file extensions (`.gb`, `.gba`, `.zip`)
- Removes region tags (`(USA)`, `(Japan)`, `(Europe)`)
- Removes version info (`(v1.1)`, `(Rev 1)`)
- Removes extra tags (`[!]`, `[T+Eng]`)
- Preserves hack indicators (`[Hack]`)

Example: `Super Mario Bros. (USA) (Rev 1).nes` → `Super Mario Bros.`

---

### Minarch Frontend

**Location**: `/workspace/all/minarch/minarch.c` (4,830 lines)

**Purpose**: Libretro frontend for running emulator cores.

#### Responsibilities

1. **Core Management**
   - Dynamic loading of libretro `.so` cores
   - Core initialization and shutdown
   - Implements libretro frontend callbacks
   - Handles core API versioning

2. **Game Execution**
   - Loads ROM files (with .zip support)
   - Manages game state
   - Main emulation loop
   - Frame timing and synchronization

3. **Save State System**
   - 10 manual save slots (0-9)
   - Auto-save slot (slot 9)
   - Quicksave on power-off
   - Auto-resume from slot 9 on boot
   - Compression with zlib
   - Save state preview (planned)

4. **In-Game Menu**
   - Pause during gameplay
   - Save/Load state management
   - Disc changing (multi-disc games)
   - Options configuration
   - Screenshot capture
   - Core reset

5. **Video Processing**
   - Receives frames from libretro core
   - Software scaling via scaler.c
   - Multiple scaling modes:
     - Native (1:1)
     - Aspect (maintain ratio)
     - Full (stretch)
     - Cropped (zoom)
   - Sharpness control (sharp/crisp/soft)
   - CRT effects (scanlines, grid)
   - Threaded video option

6. **Audio Processing**
   - Receives samples from libretro core
   - Ring buffer for low-latency audio
   - Resampling (limited)
   - Volume control

7. **Input Mapping**
   - Maps device buttons to libretro inputs
   - Per-console button configuration
   - Custom button bindings
   - Analog stick support (where available)

8. **Configuration**
   - Frontend settings (`.minarch.ini`)
   - Per-console settings (`[TAG].ini`)
   - Per-core settings (`[CORE].ini`)
   - Core options (`.opt` files)
   - Persistent configuration

#### Key Functions

```c
main()                          // Entry point
retro_init()                    // Core initialization
core_load_game()                // Load ROM
core_run()                      // Main loop
video_cb()                      // Video frame callback
audio_sample_batch_cb()         // Audio callback
input_poll_cb()                 // Input polling
saveState() / loadState()       // Save state management
showMenu()                      // In-game menu
applyVideoSettings()            // Scaling/effects
```

#### Configuration Files

**Frontend Config** (`.minarch.ini`):
```ini
volume=10
brightness=5
scale_mode=2
sharpness=1
effect=0
threaded_video=0
```

**Console Config** (`[TAG].ini`):
```ini
core=gambatte_libretro.so
aspect=1
crop=0
```

**Core Options** (`[CORE].opt`):
```ini
gambatte_gb_colorization=internal
gambatte_gb_palette=GBC - Blue
```

#### Save State Format

Save states are stored in `/Saves/[Console]/`:
```
[ROM_NAME].sav           # Save RAM (battery backup)
[ROM_NAME].st0           # Save state slot 0
[ROM_NAME].st1           # Save state slot 1
...
[ROM_NAME].st9           # Auto-save slot
```

#### Multi-Disc Games

Minarch detects `.m3u` playlists:
```
# Final Fantasy VII.m3u
Final Fantasy VII (Disc 1).bin
Final Fantasy VII (Disc 2).bin
Final Fantasy VII (Disc 3).bin
```

Disc changing available in in-game menu.

---

## Common Libraries

### Graphics API (GFX_*)

**Location**: `/workspace/all/common/api.c`

**Purpose**: Unified graphics interface for all applications.

#### Functions

```c
GFX_init()                    // Initialize graphics subsystem
GFX_quit()                    // Cleanup graphics
GFX_clear()                   // Clear screen
GFX_flip()                    // Present frame (swap buffers)
GFX_sync()                    // Wait for vsync

// Asset loading
GFX_loadImage()               // Load PNG asset
GFX_freeImage()               // Free image memory
GFX_blitImage()               // Draw image to screen

// Text rendering
GFX_openFont()                // Load TTF font
GFX_closeFont()               // Free font
GFX_drawText()                // Render text string
GFX_getTextWidth()            // Measure text width

// UI elements
GFX_drawPill()                // Rounded rectangle
GFX_drawButton()              // Button with label
GFX_drawBattery()             // Battery indicator
GFX_drawVolume()              // Volume indicator
GFX_drawBrightness()          // Brightness indicator
```

#### Features

- Asset caching for performance
- Multiple font sizes (16, 20, 24, 32, 40 pt)
- Automatic scaling based on screen resolution
- Alpha blending support
- Dirty rectangle optimization

---

### Sound API (SND_*)

**Location**: `/workspace/all/common/api.c`

**Purpose**: Audio initialization and buffering.

#### Functions

```c
SND_init()                    // Initialize audio
SND_quit()                    // Cleanup audio
SND_write()                   // Write samples to buffer
```

#### Features

- Ring buffer for low-latency
- Configurable sample rate
- Stereo output
- Lock-free design

---

### Input API (PAD_*)

**Location**: `/workspace/all/common/api.c`

**Purpose**: Button and analog stick input handling.

#### Functions

```c
PAD_init()                    // Initialize input
PAD_quit()                    // Cleanup input
PAD_poll()                    // Read current button state
PAD_justPressed()             // Check for new press
PAD_justRepeated()            // Check for auto-repeat
PAD_justReleased()            // Check for release
```

#### Button Definitions

```c
BTN_UP          // D-pad up
BTN_DOWN        // D-pad down
BTN_LEFT        // D-pad left
BTN_RIGHT       // D-pad right
BTN_A           // Face button A
BTN_B           // Face button B
BTN_X           // Face button X (if present)
BTN_Y           // Face button Y (if present)
BTN_L1          // Left shoulder
BTN_R1          // Right shoulder
BTN_L2          // Left trigger (if present)
BTN_R2          // Right trigger (if present)
BTN_SELECT      // Select/Back
BTN_START       // Start/Menu
BTN_MENU        // Menu button (if present)
BTN_L3          // Left stick press (if present)
BTN_R3          // Right stick press (if present)
```

#### Features

- Auto-repeat with configurable delay and rate
- Analog stick normalization
- Dead zone handling
- Platform-independent button IDs

---

### Power API (PWR_*)

**Location**: `/workspace/all/common/api.c`

**Purpose**: Power management and system control.

#### Functions

```c
PWR_init()                    // Initialize power subsystem
PWR_quit()                    // Cleanup power
PWR_update()                  // Update power state
PWR_getBatteryLevel()         // Get battery percentage
PWR_isCharging()              // Check charging status
PWR_sleep()                   // Enter sleep mode
PWR_wake()                    // Exit sleep mode
PWR_powerOff()                // Shutdown device
PWR_getCPUSpeed()             // Get CPU frequency
PWR_setCPUSpeed()             // Set CPU frequency
```

#### Features

- Automatic sleep after 30 seconds idle
- Auto power-off after 2 minutes asleep
- Battery level monitoring
- Charging detection
- CPU frequency scaling
- Backlight control

---

### Video Scaler (scaler.c)

**Location**: `/workspace/all/common/scaler.c` (110 KB)

**Purpose**: Software video scaling and effects.

#### Scaling Modes

1. **Native** - 1:1 pixel mapping (no scaling)
2. **Aspect** - Maintain aspect ratio, letterbox/pillarbox
3. **Full** - Stretch to fill screen
4. **Cropped** - Zoom to fill, crop edges

#### Sharpness Levels

1. **Sharp** - Nearest neighbor (crisp pixels)
2. **Crisp** - Optimized nearest neighbor
3. **Soft** - Bilinear interpolation

#### Effects

1. **None** - Clean output
2. **Scanlines** - Horizontal lines (CRT effect)
3. **Grid** - Full grid pattern (CRT effect)

#### Optimizations

- NEON SIMD intrinsics for ARM
- Fast paths for common scaling ratios
- RGB565 native format
- Integer-only math
- Loop unrolling

#### Functions

```c
scale_bitmap()                // Main scaling function
scale_nearest()               // Nearest neighbor
scale_bilinear()              // Bilinear interpolation
apply_scanlines()             // Add scanline effect
apply_grid()                  // Add grid effect
```

---

### Utilities (utils.c)

**Location**: `/workspace/all/common/utils.c`

**Purpose**: Common helper functions.

#### Functions

```c
exists()                      // Check if file exists
isDir()                       // Check if path is directory
makeDir()                     // Create directory
exactMatch()                  // Case-sensitive string match
prefixMatch()                 // Check string prefix
suffixMatch()                 // Check string suffix
getFilename()                 // Extract filename from path
getExtension()                // Get file extension
trimExtension()               // Remove extension
normalizePath()               // Clean up path
```

---

## Platform Abstraction Layer

### Platform Interface (platform.h/platform.c)

**Location**: `/workspace/[platform]/platform/`

Each platform implements a standard interface:

#### Required Functions

```c
// Video
void PLAT_initVideo(void)
void PLAT_quitVideo(void)
void PLAT_clearVideo(void)
void PLAT_clearAll(void)
void PLAT_setVsync(int enabled)
void PLAT_vsync(void)
void PLAT_flip(void)
Surface* PLAT_getScreen(void)

// Input
void PLAT_initInput(void)
void PLAT_quitInput(void)
void PLAT_pollInput(void)

// Power
int PLAT_getBatteryStatus(void)
void PLAT_enableBacklight(int enable)
void PLAT_powerOff(void)
void PLAT_setCPUSpeed(int speed)

// Scaling (optional)
void PLAT_scale(Surface* src, SDL_Rect* src_rect,
                Surface* dst, SDL_Rect* dst_rect,
                int sharp, int effect)
```

#### Platform Constants

```c
// Screen dimensions
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define FIXED_BPP 16

// Scaling
#define FIXED_SCALE 2

// Paths
#define SDCARD_PATH "/mnt/SDCARD"
#define MUTE_VOLUME_RAW 0

// Capabilities
#define HAS_HDMI 0
#define HAS_ANALOG 0
```

---

### Settings Library (libmsettings)

**Location**: `/workspace/[platform]/libmsettings/`

**Purpose**: Persistent platform settings.

#### API

```c
int InitSettings(void)
void QuitSettings(void)

int GetBrightness(void)
void SetBrightness(int value)

int GetVolume(void)
void SetVolume(int value)

char* GetLastPlay(void)
void SetLastPlay(char* path)
```

#### Implementation

Each platform implements settings using:
- `/sys/class/backlight/` for brightness
- ALSA or platform mixer for volume
- Settings file for persistence (typically `/mnt/SDCARD/.minui/settings.ini`)

---

### Input Monitor (keymon)

**Location**: `/workspace/[platform]/keymon/`

**Purpose**: Background daemon for global input handling.

#### Responsibilities

1. **Raw Input Monitoring**
   - Opens `/dev/input/event*` devices
   - Reads raw input events
   - Bypasses SDL for system-level access

2. **Global Shortcuts**
   - Volume up/down (typically L1/R1 + D-pad)
   - Brightness up/down (typically L1/R1 + D-pad)
   - Sleep trigger (typically power button short press)
   - Force shutdown (power button long press)

3. **Sleep Management**
   - Detects idle time
   - Triggers sleep mode
   - Handles wake events
   - Manages backlight state

4. **System Protection**
   - Prevents stock OS from interfering
   - Blocks unwanted input handlers
   - Ensures MinUI has input priority

#### Platform-Specific

Each platform has different button codes and input devices:
- Miyoo Mini: `/dev/input/event0`
- RG35XX: Multiple event devices
- Trimui: Different GPIO mapping

---

## Utility Programs

### Clock

**Location**: `/workspace/all/clock/clock.c`

Simple clock display showing current time and date. Uses platform settings for rendering.

### Minput

**Location**: `/workspace/all/minput/minput.c`

Input testing tool:
- Shows button presses in real-time
- Displays analog stick values
- Helps diagnose input issues
- Useful for button remapping

### Say

**Location**: `/workspace/all/say/say.c`

Notification system:
- Displays text messages on screen
- Used by system scripts
- Splash screen alternative
- Simple text overlay

### Syncsettings

**Location**: `/workspace/all/syncsettings/syncsettings.c`

Settings restoration daemon:
- Monitors for third-party app launches
- Restores MinUI brightness/volume after exit
- Prevents RetroArch or other apps from changing settings permanently
- Runs in background

---

## Libretro Cores

### Core Building

**Location**: `/workspace/all/cores/makefile`

Template-based build system:

```makefile
# Example core definition
$(CORE_DIR)/fceumm_libretro.so:
    git clone https://github.com/libretro/fceumm $(SRC_DIR)/fceumm
    cd $(SRC_DIR)/fceumm && git checkout [commit-hash]
    # Apply patches
    $(MAKE) -C $(SRC_DIR)/fceumm -f Makefile platform=[platform]
    cp $(SRC_DIR)/fceumm/fceumm_libretro.so $(CORE_DIR)/
```

### Stock Cores

1. **FCEUmm** - NES/Famicom
   - Fast, accurate NES emulation
   - Mapper support for most games
   - Save state support

2. **Gambatte** - Game Boy / Game Boy Color
   - High accuracy
   - Color palettes
   - Super Game Boy support

3. **GPSP** - Game Boy Advance
   - Fast ARM-based dynarec
   - Good compatibility
   - Save state support

4. **PicoDrive** - Sega Genesis/CD, Master System, Game Gear
   - Fast 68000/Z80 emulation
   - Sega CD support
   - Multiple systems in one core

5. **Snes9x2005 Plus** - Super Nintendo
   - Balance of speed and accuracy
   - Enhanced version of Snes9x 1.43
   - Good compatibility

6. **PCSX ReARMed** - PlayStation
   - ARM-optimized dynarec
   - Strong compatibility
   - Multi-disc support
   - Memory card emulation

### Extra Cores

Available in extras package:
- MGBA - Better GBA accuracy
- Mednafen cores - High accuracy multi-system
- Fake08 - PICO-8 fantasy console
- Race - Neo Geo Pocket
- PokeMini - Pokemon Mini

---

## Pak System

**Location**: Various `.pak/` directories

### Structure

```
[NAME].pak/
├── launch.sh              # Launch script
├── config.json            # Configuration (optional)
└── [additional files]     # Core, assets, etc.
```

### Launch Script

```bash
#!/bin/sh
# Example launch.sh for emulator pak

ROM="$1"
CORE="/mnt/SDCARD/SYSTEM/[platform]/cores/[core]_libretro.so"

cd "$(dirname "$0")"
/mnt/SDCARD/SYSTEM/[platform]/bin/minarch.elf "$CORE" "$ROM"
```

### Configuration

```json
{
    "name": "Game Boy",
    "core": "gambatte_libretro.so",
    "options": {
        "gambatte_gb_colorization": "internal",
        "gambatte_gb_palette": "GBC - Blue"
    }
}
```

### Types of Paks

1. **Emulator (Reuse Core)** - Uses system core
2. **Emulator (Custom Core)** - Bundles own core
3. **Native Tool** - Standalone application
4. **Script Tool** - Shell script utility

See [PAKS.md](../PAKS.md) for complete pak creation guide.

---

## Component Dependencies

```
minui.elf depends on:
  - api.c (GFX, PAD, PWR)
  - utils.c
  - platform.c
  - libmsettings.so
  - SDL, SDL_image, SDL_ttf

minarch.elf depends on:
  - api.c (GFX, SND, PAD, PWR)
  - scaler.c
  - utils.c
  - platform.c
  - libmsettings.so
  - SDL, zlib, dlopen

keymon.elf depends on:
  - libmsettings.so
  - raw input devices

libmsettings.so depends on:
  - ALSA (audio)
  - sysfs (backlight)
  - file I/O
```

## Performance Characteristics

| Component | CPU Usage | Memory | Disk I/O |
|-----------|-----------|--------|----------|
| minui | Very Low | ~2 MB | Low (ROM scan) |
| minarch | Medium-High | ~20 MB | Low (save states) |
| keymon | Very Low | <1 MB | None |
| syncsettings | Very Low | <1 MB | Very Low |
| scaler | High | Minimal | None |

## Thread Model

Most components are single-threaded:
- minui - Single thread
- minarch - Main thread + optional video thread
- keymon - Single thread
- syncsettings - Single thread

Synchronization via file locking where needed.
