# Anbernic RGB30

Platform implementation for the Anbernic RGB30 retro handheld device.

## Hardware Specifications

### Display
- **Resolution**: 720x720 (square display, 1:1 aspect ratio)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **HDMI Output**: 1280x720 (720p) with automatic switching

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **Analog Sticks**: L3, R3 (clickable, used as menu modifiers)
- **System Buttons**:
  - Two MENU buttons (primary and alternate via L3/R3)
  - POWER button
  - Dedicated PLUS/MINUS volume buttons
  - SELECT and START buttons

### Input Method
- **Primary**: SDL Joystick API (gamepad input)
- **Secondary**: Evdev codes for volume, power, and menu modifier buttons
- **Hybrid Design**: Minimal SDL keyboard (power button only), most input via joystick

### CPU & Performance
- ARM processor with NEON SIMD support
- No overclocking support

### Power Management
- Power button detection via SDL keyboard (SDLK_POWER)
- Sleep/wake functionality

### Audio/Video
- **Headphone Jack**: Automatic detection and audio routing
- **HDMI Output**: Automatic detection with video/audio switching
- **Volume Range**: 0-20 (raw: 0-100 via amixer)
- **Brightness Range**: 0-10 (raw: 0-255)

### Storage
- SD card mounted at `/storage/roms`

## Platform Architecture

The RGB30 uses a **hybrid input system** that combines:
- SDL2 joystick for gamepad buttons (D-Pad, face buttons, shoulders)
- Minimal SDL keyboard for power button
- Evdev codes for volume control and L3/R3 menu modifiers

This approach allows hardware volume buttons to work independently while maintaining standard gamepad compatibility.

## Directory Structure

```
rgb30/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, HDMI support
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness + headphone/HDMI detection
├── libmsettings/      Settings library with audio/video routing
│   ├── msettings.c    Shared memory settings management
│   └── msettings.h    Settings API (volume, brightness, jack, HDMI)
├── show/              Boot splash screen utility
│   ├── show.c         SDL2-based image display (supports HDMI)
│   └── makefile       Build configuration
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    ├── DinguxCommander/  File manager (branch: minui-rgb30)
    └── sdl12-compat/     SDL 1.2 compatibility layer (branch: minui-rgb30)
```

## Input System

The RGB30 uses a **joystick-first input architecture**:

1. **SDL Joystick API**: Primary gamepad input (D-Pad, face buttons, shoulders, Start/Select)
2. **SDL Keyboard**: Power button only (SDLK_POWER)
3. **Evdev Codes**: Volume buttons (114/115) and L3/R3 menu modifiers (317/318)

### Joystick Button Mappings

| Physical Button | Joystick Index |
|----------------|----------------|
| D-Pad Up/Down/Left/Right | 13/14/15/16 |
| A, B, X, Y | 1, 0, 2, 3 |
| L1, R1 | 4, 5 |
| L2, R2 | 6, 7 |
| SELECT, START | 8, 9 |
| MENU (primary) | 11 |
| MENU ALT (secondary) | 12 |

### Input Event Devices

The keymon daemon monitors multiple input devices:
- **event0-4**: Gamepad and button input
- **js0**: Joystick device

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| L3/R3 + PLUS | Increase brightness |
| L3/R3 + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Hardware Monitoring

The keymon daemon runs a background thread to monitor:
- **Headphone jack state**: `/sys/bus/platform/devices/singleadc-joypad/hp`
- **HDMI connection state**: `/sys/class/extcon/hdmi/cable.0/state`

Audio and video routing automatically switches when headphones or HDMI are connected/disconnected.

## Building

### Prerequisites
Requires Docker with RGB30 cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=rgb30 shell

# Inside container: build all platform components
cd /root/workspace/rgb30
make

# This builds (in early target):
# - DinguxCommander (file manager)
# - sdl12-compat (SDL 1.2 compatibility layer)
#
# And (in all target):
# - show.elf (boot splash display)
# - All libretro cores in cores/
```

### Dependencies

The platform automatically clones required dependencies on first build:
- **DinguxCommander**: `github.com/shauninman/DinguxCommander.git` (branch: `minui-rgb30`)
- **sdl12-compat**: `github.com/shauninman/sdl12-compat.git` (branch: `minui-rgb30`)

Both dependencies use custom toolchain configurations for the RGB30 hardware.

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/storage/roms/
├── .system/
│   ├── rgb30/              Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, show, etc.)
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── .userdata/              User settings and save data
│   └── rgb30/              Platform-specific settings
│       └── msettings.bin   Volume/brightness preferences
├── Roms/                   ROM files organized by system
└── MinUI.zip               Update package (if present)
```

### Settings Management

The RGB30 uses shared memory for settings synchronization between processes:

**Settings Stored**:
- Brightness level (0-10)
- Speaker volume (0-20)
- Headphone volume (0-20, separate from speaker)
- Jack state (headphones connected/disconnected)
- HDMI state (HDMI connected/disconnected)

**Persistence**: Settings are saved to `$USERDATA_PATH/msettings.bin` and persist across reboots.

## Platform-Specific Features

### Square Display

The 720x720 square display (1:1 aspect ratio) requires UI adjustments:

| Parameter | Value | Notes |
|-----------|-------|-------|
| `MAIN_ROW_COUNT` | 8 rows | More rows visible than standard 640x480 |
| `PADDING` | 40px | Larger padding for square layout |

These settings maximize content visibility while maintaining comfortable spacing.

### HDMI Output Support

The platform includes full HDMI support:

**Output Resolution**: 1280x720 (720p)

**Automatic Behavior**:
- Video output switches to HDMI when connected
- Audio routing changes to HDMI audio
- Volume locked at 100% on HDMI (brightness control disabled)
- Automatically reverts to internal display/speaker when disconnected

**Implementation**: Background thread in keymon polls HDMI state every second.

### Audio Routing

Smart audio routing based on connected devices:

1. **Internal Speaker**: Default audio output
2. **Headphones**: Auto-detected, switches to headphone output, uses separate volume level
3. **HDMI**: Auto-detected, switches to HDMI audio, locks volume at 100%

**amixer Commands**:
- Playback path: `amixer cset name='Playback Path' 'SPK'|'HP'`
- Volume: `amixer sset -M 'Master' N%`

### Brightness Control

Hardware brightness via sysfs:
- **Path**: `/sys/class/backlight/backlight/brightness`
- **Range**: 0-255 (raw), mapped from 0-10 user scale
- **Disabled on HDMI**: Brightness adjustments ignored when HDMI connected

**Brightness Curve** (non-linear for better low-end visibility):
```
Level 0: 4     Level 1: 6      Level 2: 10     Level 3: 16
Level 4: 32    Level 5: 48     Level 6: 64     Level 7: 96
Level 8: 128   Level 9: 192    Level 10: 255
```

### Volume Button Quirks

The platform has hardware volume buttons with evdev codes:
- **CODE_PLUS**: 129 (Volume up) - **swapped from kernel default**
- **CODE_MINUS**: 128 (Volume down) - **swapped from kernel default**

These codes are swapped in platform.h to correct the hardware mapping.

### Repeat Functionality

Volume and brightness buttons support repeat:
- **Initial delay**: 300ms before repeat starts
- **Repeat interval**: 100ms between repeats
- **Ignores stale input**: After system sleep (>1 second gap), input ignored to prevent spurious events

## Supported Emulator Cores

The RGB30 platform includes these libretro cores:

| Core | Systems | Notes |
|------|---------|-------|
| **fceumm** | NES, Famicom | Nintendo Entertainment System |
| **gambatte** | Game Boy, Game Boy Color | Accuracy-focused |
| **gpsp** | Game Boy Advance | Fast, requires BIOS |
| **pcsx_rearmed** | PlayStation | Optimized ARM port |
| **picodrive** | Genesis, Mega Drive, 32X, Sega CD | Multi-system |
| **snes9x2005_plus** | SNES | Lightweight, Blargg APU |
| **pokemini** | Pokemon Mini | Niche handheld |
| **fake-08** | PICO-8 | Fantasy console |
| **mednafen_pce_fast** | PC Engine, TurboGrafx-16 | Fast variant |
| **mednafen_vb** | Virtual Boy | Nintendo VR handheld |
| **mednafen_supafaust** | SNES | High accuracy |
| **mgba** | Game Boy Advance | High accuracy |
| **race** | Neo Geo Pocket, Neo Geo Pocket Color | SNK handheld |

## Included Tools

### Files.pak
DinguxCommander-based file manager (custom RGB30 build):
- Full file operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

## Known Issues / Quirks

### Hardware Quirks
1. **Joystick-first input**: Unlike keyboard-based platforms, uses SDL joystick API primarily
2. **Dual menu buttons**: L3/R3 analog stick clicks act as menu modifiers (codes 317/318)
3. **Volume code swap**: Hardware volume button codes swapped in software to correct mapping
4. **HDMI volume lock**: Volume/brightness controls disabled when HDMI connected
5. **Audio crackling**: Light crackling reported in some cores (fceumm, snes9x2005_plus) - likely SDL2 audio pipeline issue

### Development Notes
1. **SDL2**: Platform uses SDL2 (not SDL 1.2), requiring sdl12-compat for legacy cores
2. **Shared memory settings**: libmsettings uses `/SharedSettings` SHM for IPC
3. **Settings host**: keymon is always the settings "host" (first to create SHM)
4. **HDMI polling**: Background thread polls HDMI state at 1Hz (every second)
5. **60Hz input polling**: Main keymon loop runs at 60Hz (16.6ms sleep)

### Input Limitations
- **No L3/R3 for games**: L3/R3 reserved for system menu modifier, not available to cores
- **Evdev overhead**: Volume buttons require evdev monitoring (can't use SDL alone)

### Audio Buffer Size
Default audio buffer: `SAMPLES 400` (balanced latency vs stability)

## Testing

When testing changes:
1. Verify joystick input works correctly (D-Pad, face buttons, shoulders)
2. Test HDMI detection and automatic video/audio switching
3. Check headphone jack detection and audio routing
4. Verify volume control with PLUS/MINUS buttons
5. Test brightness control with L3/R3 + PLUS/MINUS
6. Confirm settings persist across reboots (msettings.bin)
7. Test on both internal display and HDMI output
8. Verify square display UI layout (720x720)

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)

## Maintainer Notes

This platform demonstrates several advanced MinUI features:

**Unique Characteristics**:
- Square display (720x720) - unique aspect ratio among MinUI platforms
- HDMI output support with automatic switching
- Dual menu modifier buttons (L3/R3)
- Hybrid input system (joystick + keyboard + evdev)
- Background hardware monitoring threads
- Shared memory settings management
- Separate volume levels for speaker vs headphones

**Reference Implementation For**:
- HDMI output detection and switching
- Headphone jack detection
- Multi-device audio routing
- Non-standard display aspect ratios
- SDL2-based platforms
- Joystick-first input handling

Changes to this platform should be tested with HDMI output and headphone jack to ensure automatic switching continues to work correctly.
