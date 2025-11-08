# TG5040 (Trimui Smart Pro / Brick)

Platform implementation for the TG5040 handheld devices, supporting both the standard Trimui Smart Pro and the Brick variant.

## Hardware Specifications

### Display
- **Standard Resolution**: 1280x720 (HD widescreen)
- **Brick Resolution**: 1024x768 (XGA, 4:3 aspect ratio)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x standard, 3x Brick (uses `assets@2x.png` or `assets@3x.png`)
- **Variant Detection**: Runtime detection via `is_brick` flag

### Input
- **D-Pad**: Up, Down, Left, Right (via HAT)
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1
- **Analog Triggers**: L2, R2 (axis-based)
- **Analog Sticks**: Left stick (LX/LY) and Right stick (RX/RY)
- **L3/R3 Buttons**: Brick variant only (clickable analog sticks)
- **System Buttons**:
  - SELECT and START buttons
  - MENU button
  - POWER button (HOME key, code 102)
  - PLUS/MINUS volume buttons (hardware buttons on Brick, evdev codes on standard)

### Input Method
- **Primary**: SDL Joystick API (not keyboard events)
- **D-Pad**: Implemented via joystick HAT
- **Analog**: 6 analog axes (left/right sticks, L2/R2 triggers)
- **Evdev Codes**: Volume/power buttons only

### CPU & Performance
- ARM processor with NEON SIMD support
- CPU frequency: 2.0 GHz (userspace governor)
- Performance mode set during boot

### Power Management
- Hardware mute switch (GPIO243)
- Headphone jack detection via EV_SW events
- Auto-sleep and power-off features
- Battery monitoring (platform-specific)

### Storage
- SD card mounted at `/mnt/SDCARD`
- Brick variant also mounts `/mnt/UDISK`

### Audio
- ALSA mixer controls via `amixer`
- DAC channel swap correction (fixes L/R channels)
- Hardware mute switch with GPIO monitoring
- Headphone jack detection with automatic volume switching
- Volume range: 0-20 (raw: 0-100)
- Mute volume: 0

## Platform Variants

This platform supports two hardware variants detected at runtime:

### Standard Trimui Smart Pro
- **Display**: 1280x720 (16:9 widescreen)
- **Scale**: 2x
- **Main Rows**: 8
- **Padding**: 40px
- **Volume Buttons**: Evdev codes 128 (PLUS) and 129 (MINUS)
- **No L3/R3**: Clickable stick buttons not available

### Trimui Brick
- **Display**: 1024x768 (4:3 aspect ratio)
- **Scale**: 3x
- **Main Rows**: 7
- **Padding**: 5px
- **Volume Buttons**: Joystick buttons 14 (PLUS) and 13 (MINUS)
- **L3/R3**: Buttons 9 and 10 respectively
- **LED Control**: Additional LED strips (max_scale_lr, max_scale_f1f2)

**Detection**: The boot script checks for "Trimui Brick" string in `/usr/trimui/bin/MainUI`. The `is_brick` flag is set via `DEVICE` environment variable and propagates to all platform code.

## Directory Structure

```
tg5040/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, variant detection
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control, mute/jack monitoring
├── libmsettings/      Settings library (volume, brightness, jack state)
│   └── msettings.c    Dual-variant brightness/volume tables
├── show/              Boot splash screen display (SDL2)
│   └── show.c         PNG image loader for install/update screens
├── install/           Installation assets and boot scripts
│   ├── boot.sh        Variant detection and boot/update handler
│   ├── update.sh      Migration script (tg3040 to tg5040)
│   ├── installing.png Standard boot splash (1280x720)
│   ├── updating.png   Standard update splash (1280x720)
│   └── brick/         Brick-specific assets
│       ├── installing.png  Brick boot splash (1024x768)
│       └── updating.png    Brick update splash (1024x768)
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    ├── DinguxCommander-sdl2/  File manager (SDL2)
    ├── evtest/        Input device testing tool
    ├── jstest/        Joystick testing tool
    └── unzip60/       ZIP extraction utility
```

## Input System

The TG5040 uses **joystick-based input**:

1. **Joystick API**: Primary input method (D-pad via HAT, buttons via indices)
2. **Analog Axes**: 6 axes for sticks and triggers
3. **Evdev Codes**: Only for POWER (102), PLUS/MINUS volume buttons
4. **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### Button Mappings

| Physical Button | Standard Mapping | Brick Mapping |
|----------------|------------------|---------------|
| D-Pad | HAT (no button index) | HAT (no button index) |
| A | JOY 1 | JOY 1 |
| B | JOY 0 | JOY 0 |
| X | JOY 3 | JOY 3 |
| Y | JOY 2 | JOY 2 |
| L1 | JOY 4 | JOY 4 |
| R1 | JOY 5 | JOY 5 |
| L2 | Axis 2 (ABSZ) | Axis 2 (ABSZ) |
| R2 | Axis 5 (RABSZ) | Axis 5 (RABSZ) |
| L3 | N/A | JOY 9 |
| R3 | N/A | JOY 10 |
| SELECT | JOY 6 | JOY 6 |
| START | JOY 7 | JOY 7 |
| MENU | JOY 8 | JOY 8 |
| PLUS | Code 128 | JOY 14 |
| MINUS | Code 129 | JOY 13 |
| POWER | Code 102 | Code 102 |

### Analog Axes

| Axis | Control | Range |
|------|---------|-------|
| 0 (ABS_X) | Left stick X | -32k to +32k |
| 1 (ABS_Y) | Left stick Y | -32k to +32k |
| 2 (ABSZ) | L2 trigger | 0 to max |
| 3 (ABS_RX) | Right stick X | -32k to +32k |
| 4 (ABS_RY) | Right stick Y | -32k to +32k |
| 5 (RABSZ) | R2 trigger | 0 to max |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Input Quirks
- **A/B Swap**: Button mappings were swapped in first public stock release
- **HAT D-Pad**: D-pad uses joystick HAT instead of individual buttons
- **Analog Triggers**: L2/R2 are analog axes, not digital buttons
- **Variant Buttons**: L3/R3 and volume buttons differ between variants

## Building

### Prerequisites
Requires Docker with TG5040 cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=tg5040 shell

# Inside container: build all platform components
cd /root/workspace/tg5040
make

# This builds:
# - keymon (button monitoring daemon)
# - libmsettings (settings library with variant support)
# - show.elf (SDL2 boot splash display)
# - DinguxCommander (file manager)
# - evtest (input testing tool)
# - jstest (joystick testing tool)
# - unzip (ZIP extraction)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **DinguxCommander-sdl2**: `github.com/shauninman/DinguxCommander-sdl2.git`
- **evtest**: `github.com/freedesktop-unofficial-mirror/evtest.git`
- **jstest**: `github.com/datrh/joyutils.git`
- **unzip60**: `github.com/shauninman/unzip60.git`

### Supported Libretro Cores
- **NES**: fceumm
- **Game Boy / GBC**: gambatte
- **GBA**: gpsp, mgba
- **SNES**: snes9x2005_plus (with Blargg APU)
- **Genesis/Mega Drive**: picodrive
- **PC Engine/TurboGrafx-16**: mednafen_pce_fast
- **PlayStation**: pcsx_rearmed
- **Virtual Boy**: mednafen_vb
- **SNES (enhanced)**: mednafen_supafaust
- **PICO-8**: fake-08
- **Pokemon Mini**: pokemini
- **Neo Geo Pocket**: race

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── tg5040/             Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, etc.)
│   │   │   └── install.sh  Post-update script
│   │   ├── dat/            Data files (runtrimui.sh)
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale - standard)
│       ├── assets@3x.png   UI sprite sheet (3x scale - Brick)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/            Update staging area
│   └── tg5040/             Platform boot components
│       ├── show.elf        Splash screen display
│       ├── unzip           ZIP extraction utility
│       ├── installing.png  Standard install splash (1280x720)
│       ├── updating.png    Standard update splash (1280x720)
│       └── brick/          Brick variant assets
│           ├── installing.png  Brick install splash (1024x768)
│           └── updating.png    Brick update splash (1024x768)
├── .userdata/              User settings and saves
│   └── tg5040/             Platform-specific settings
├── Roms/                   ROM files organized by system
└── MinUI.zip               Update package (if present)
```

### Boot Process

1. Device boots and runs `.tmp_update/tg5040.sh` (boot.sh)
2. Script remounts SD cards as read-write (Brick has `/mnt/UDISK` too)
3. Sets CPU governor to "userspace" and frequency to 2.0 GHz
4. Detects variant by checking MainUI binary for "Trimui Brick" string
5. If `MinUI.zip` exists:
   - Disables LED animations (standard + Brick-specific LEDs if applicable)
   - Displays variant-appropriate splash screen:
     - Standard: `installing.png` or `updating.png` (1280x720)
     - Brick: `brick/installing.png` or `brick/updating.png` (1024x768)
   - Extracts `MinUI.zip` to SD card
   - Deletes ZIP file
   - Runs `.system/tg5040/bin/install.sh` to complete setup
   - Reboots if fresh install
6. Launches MinUI via `.system/tg5040/paks/MinUI.pak/launch.sh`

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root (`/mnt/SDCARD/`)
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. Displays variant-appropriate update splash
5. ZIP is deleted after successful extraction

### Migration from TG3040

The update script (`update.sh`) handles migration from the old Brick platform identifier:

1. Checks for `/mnt/SDCARD/.system/tg3040` (old Brick folder)
2. If found:
   - Copies all `.cfg` files from `.userdata/tg3040/*/*.cfg` to `.userdata/tg5040/*/NAME-brick.cfg`
   - Deletes old system folder and userdata
   - Removes old update staging area
   - Reboots to complete migration
3. Updates `runtrimui.sh` if outdated version detected

## Platform-Specific Features

### Variant Detection

Runtime detection is performed in multiple layers:

1. **Boot Script** (`boot.sh`):
   - Reads `/usr/trimui/bin/MainUI` binary
   - Searches for "Trimui Brick" string
   - Sets `DEVICE=brick` environment variable
   - Loads variant-specific splash screens

2. **Settings Library** (`libmsettings`):
   - Reads `DEVICE` environment variable
   - Sets `is_brick` flag
   - Uses variant-specific brightness tables

3. **Platform Header** (`platform.h`):
   - Declares `extern int is_brick`
   - Uses ternary operators for runtime configuration:
     - `FIXED_SCALE = is_brick ? 3 : 2`
     - `FIXED_WIDTH = is_brick ? 1024 : 1280`
     - `JOY_L3 = is_brick ? 9 : JOY_NA`

### Audio Configuration

The platform performs extensive ALSA mixer initialization:

```bash
amixer sset 'Headphone' 0       # 100% headphone volume
amixer sset 'digital volume' 0  # 100% digital volume
amixer sset 'DAC Swap' Off      # Fix L/R channel swap
```

Volume control uses inverted digital volume mapping:
- User volume 0-20 maps to raw 0-100
- `digital volume` is set to `100 - raw_value` (reversed mapping)
- At mute: Both `digital volume` and `DAC volume` set to 0
- Otherwise: `DAC volume` fixed at 160 (0dB)

### Brightness Control

Variant-specific brightness tables provide non-linear scaling:

| Level | Standard Raw | Brick Raw | Description |
|-------|-------------|-----------|-------------|
| 0 | 4 | 1 | Minimum (not off) |
| 1 | 6 | 8 | Very dim |
| 2 | 10 | 16 | Dim |
| 3 | 16 | 32 | Low |
| 4 | 32 | 48 | Medium-low |
| 5 | 48 | 72 | Medium |
| 6 | 64 | 96 | Medium-high |
| 7 | 96 | 128 | High |
| 8 | 128 | 160 | Higher |
| 9 | 192 | 192 | Very high |
| 10 | 255 | 255 | Maximum |

Brightness is set via `/dev/disp` ioctl (`DISP_LCD_SET_BRIGHTNESS = 0x102`).

### Hardware Monitoring

The keymon daemon monitors multiple hardware states:

1. **Mute Switch** (background thread):
   - Polls GPIO243 (`/sys/class/gpio/gpio243/value`) every 200ms
   - Updates mute state when switch changes
   - Runs in separate pthread until quit signal

2. **Headphone Jack** (EV_SW events):
   - Monitors switch event code 2 (CODE_JACK)
   - Automatically switches between headphone/speaker volume
   - Maintains separate volume settings for each output

3. **Button Events** (EV_KEY):
   - 60Hz polling of event0-3 input devices
   - Implements repeat functionality (300ms initial delay, 100ms interval)
   - Ignores stale input after >1 second gap (prevents spurious events after sleep)

### LED Control

Brick variant has additional LED strips controlled via sysfs:
- `/sys/class/led_anim/max_scale` - Main LED brightness
- `/sys/class/led_anim/max_scale_lr` - L/R shoulder LEDs (Brick only)
- `/sys/class/led_anim/max_scale_f1f2` - Front LEDs (Brick only)

LEDs are disabled during boot/update process (set to 0).

### Boot Splash Display

The `show.elf` utility uses SDL2 to display PNG images:
- Loads variant-appropriate PNG from boot script
- Centers image on screen
- Default 2-second display time (configurable)
- Handles any resolution (screen dimensions auto-detected)

## UI Layout Adjustments

The Brick variant uses different UI parameters to optimize for its display:

| Parameter | Standard (1280x720) | Brick (1024x768) |
|-----------|-------------------|------------------|
| `FIXED_SCALE` | 2x | 3x |
| `FIXED_WIDTH` | 1280px | 1024px |
| `FIXED_HEIGHT` | 720px | 768px |
| `MAIN_ROW_COUNT` | 8 rows | 7 rows |
| `PADDING` | 40px | 5px |

These adjustments account for the aspect ratio difference (16:9 vs 4:3) and different UI scaling.

## Included Tools

### Files.pak
DinguxCommander-sdl2 file manager with:
- SDL2 graphics (hardware accelerated)
- Full file operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

### evtest
Hardware input testing utility:
- Displays raw input events from kernel
- Useful for debugging button/axis mappings
- Shows switch events (jack, mute)

### jstest
Joystick testing utility:
- Tests joystick buttons and axes
- Calibration support
- Useful for verifying analog stick behavior

## Known Issues / Quirks

### Hardware Quirks
1. **Button Label Swap**: A/B and X/Y physical labels don't match software mappings (first stock firmware had them swapped)
2. **Inverted Volume**: ALSA `digital volume` control is reversed (100 = silent, 0 = loud)
3. **Dual Mute Control**: Muting requires setting both `digital volume` and `DAC volume` to prevent audio leakage
4. **Variant Detection**: Relies on binary string search in MainUI (fragile)
5. **GPIO Polling**: Mute switch requires constant GPIO polling (no interrupt support)
6. **Headphone Boot State**: Cannot detect headphone jack state at boot, only detects change events (separate speaker/headphone volumes less useful)
7. **WiFi Limitations**: Stock `wget` lacks SSL support; Splore.pak cannot download carts from BBS (workaround: use `curl` instead)
8. **PSP/NDS Setup**: Requires copying assets from stock firmware (not included with MinUI)

### Platform Differences
- **L3/R3 Support**: Only Brick variant has clickable analog sticks
- **Volume Buttons**: Different implementation (evdev codes vs joystick buttons)
- **LED Strips**: Brick has additional LED controllers
- **Display Ratio**: 16:9 (standard) vs 4:3 (Brick) affects UI layout
- **Mount Points**: Brick has additional `/mnt/UDISK` mount

### Development Notes
1. **HAT D-Pad**: D-pad input uses HAT, not individual button indices
2. **Analog Triggers**: L2/R2 are axes (ABSZ/RABSZ), not digital buttons
3. **Runtime Configuration**: Many constants use ternary operators based on `is_brick`
4. **Multiple Input Devices**: Must monitor event0-3 for complete input coverage
5. **Stale Input Prevention**: 1-second gap detection prevents phantom button presses after sleep
6. **NEON Optimizations**: Platform supports ARM NEON SIMD - use `HAS_NEON` define

### Input Quirks
- **No Keyboard Input**: Platform exclusively uses joystick API
- **MENU Button Codes**: Multiple evdev codes (314, 315, 316) all map to MENU
- **Button Repeat**: Only PLUS/MINUS buttons have repeat behavior (300ms delay, 100ms interval)
- **Analog Range**: Sticks use full 16-bit signed range (-32k to +32k)

## Testing

When testing changes:
1. Test on **both** standard and Brick variants
2. Verify resolution-specific assets load correctly (720p vs 768p)
3. Check variant-specific button mappings (L3/R3, volume buttons)
4. Test brightness tables (different raw values per variant)
5. Verify boot splash screens display at correct resolution
6. Test headphone jack switching between speaker/headphone volumes
7. Verify mute switch GPIO monitoring works
8. Check LED control (especially Brick-specific LEDs)
9. Test analog stick and trigger functionality
10. Verify HAT-based D-pad input works correctly

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)

## Maintainer Notes

This platform is notable for:
- **Dual-variant support**: Runtime detection of two different hardware configurations
- **Joystick-exclusive input**: No SDL keyboard events, uses HAT for D-pad
- **Analog controls**: Full analog stick and trigger support
- **Complex audio**: Inverted volume mapping, dual mute controls, jack detection
- **Hardware monitoring**: GPIO polling, switch events, multi-device input
- **Migration support**: Handles upgrade from old tg3040 platform identifier

The variant detection mechanism is somewhat fragile (depends on binary string search), but allows a single build to support both devices. Future improvements might include more robust hardware detection.

Changes to input handling should be tested with analog controllers and HAT-based D-pad input, as this differs significantly from keyboard-based platforms.
