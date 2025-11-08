# Mini Zero 28 (MagicX Mini Zero 28)

Platform implementation for the Mini Zero 28 retro handheld device.

> **Note**: This platform is marked as deprecated in the main MinUI distribution. Support may be limited for newer features.

## Hardware Specifications

### Display
- **Resolution**: 640x480 (VGA)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Orientation**: Auto-detects portrait mode and applies 90-degree rotation when height > width
- **Hardware**: `/dev/disp` ioctl-based backlight and brightness control

### Input
- **D-Pad**: Up, Down, Left, Right (via joystick HAT)
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **Analog Sticks**: Left (L3 clickable) and Right (R3 clickable)
- **System Buttons**:
  - MENU button
  - POWER button (code 102 - KEY_HOME)
  - Dedicated PLUS/MINUS volume buttons

**Note**: A/B and X/Y button mappings were swapped in the first public stock release.

### Input Method
- **Primary**: SDL Joystick API (joystick index 0)
- **HAT Input**: D-pad uses HAT events (indices 13-16)
- **Evdev**: Volume/brightness keys monitored via `/dev/input/event2` and `/dev/input/event3`
- **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### CPU & Performance
- ARM processor with NEON SIMD support
- Four CPU speed profiles:
  - **MENU**: 600 MHz (UI navigation)
  - **POWERSAVE**: 816 MHz (low-demand games)
  - **NORMAL**: 1416 MHz (most games)
  - **PERFORMANCE**: 1800 MHz (demanding games)
- Governor control via `/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed`

### Power Management
- **Battery**: AXP2202 power management IC
- **Battery path**: `/sys/class/power_supply/axp2202-battery/capacity`
- **Charging detection**: `/sys/class/power_supply/axp2202-usb/online`
- **WiFi monitoring**: `/sys/class/net/wlan0/operstate` (polled during battery checks)

### Storage
- SD card mounted at `/mnt/SDCARD`

### Audio
- **Volume Scale**: Inverted (63 = mute, 0 = max volume)
- **Sample Rate**: 400 samples (buffer size tuned for fceumm compatibility)
- **Mixer Controls**:
  - "Headphone" - set to 0 (100%)
  - "digital volume" - set to 0 (100%)
  - "Soft Volume Master" - set to 255 (100%)
  - "DAC volume" - active volume control (0 or 96-160)
- **Headphone jack detection**: Via EV_SW switch events (CODE_JACK = 2)
- **Separate volume levels**: Maintains independent speaker/headphone volume settings

## Platform Architecture

The Zero 28 platform uses several unique features:

### SDL2 Joystick Input
Unlike many MinUI platforms that use SDL keyboard events, the Zero28 uses SDL_Joystick API for all gamepad input. The D-pad uses joystick HAT events rather than discrete button events.

### Hardware-Accelerated Rendering
The platform leverages SDL2's hardware-accelerated renderer with support for:
- **Crisp rendering**: Nearest neighbor upscale + bilinear downscale
- **Soft rendering**: Pure bilinear scaling
- **Visual effects**: Grid and line overlays for CRT simulation
- **Display rotation**: Automatic 90-degree rotation for portrait displays

### Backlight Control
Uses external `bl_enable` and `bl_disable` binaries that communicate with `/dev/disp` via ioctl commands (`DISP_LCD_BACKLIGHT_ENABLE` / `DISP_LCD_BACKLIGHT_DISABLE`).

## Directory Structure

```
zero28/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs, paths
│   └── platform.c     Platform implementation (883 lines)
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness via MENU+PLUS/MINUS
├── libmsettings/      Settings library (volume, brightness, jack)
│   ├── msettings.h    Settings API header
│   └── msettings.c    Shared memory settings implementation
├── bl/                Backlight enable/disable utilities
│   ├── bl_enable.c    Enable backlight via ioctl
│   └── bl_disable.c   Disable backlight via ioctl
├── show/              Boot splash screen display
│   └── show.c         SDL2-based PNG display with rotation support
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler (becomes zero28.sh)
│   ├── installing.png Boot splash for fresh install (640x480)
│   └── updating.png   Boot splash for updates (640x480)
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    └── DinguxCommander-sdl2/ File manager (SDL2 fork)
```

## Input System

The Zero28 uses a **joystick-centric input architecture**:

1. **SDL Joystick API**: All gamepad input via `SDL_JoystickOpen(0)`
2. **HAT D-Pad**: D-pad uses joystick HAT (indices 13-16, not discrete buttons)
3. **Evdev Monitoring**: `keymon` watches `/dev/input/event2` and `/dev/input/event3` for volume/brightness keys
4. **No SDL Keyboard**: Platform does not use SDL keyboard events (`BUTTON_NA` for all keyboard mappings)

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Input Event Devices
- **event2**: Gamepad input (face buttons, shoulders, menu)
- **event3**: Volume buttons (PLUS/MINUS)

### Hardware Button Details
- **MENU button code**: 158 (CODE_MENU in keymon)
- **Volume up code**: 115 (CODE_PLUS in keymon)
- **Volume down code**: 114 (CODE_MINUS in keymon)
- **Headphone jack switch**: 2 (CODE_JACK, generates EV_SW events)
- **Volume range**: 0-20 (separate speaker/headphone settings)
- **Brightness range**: 0-10 (non-linear mapping to 1-255 raw values)

## Building

### Prerequisites
Requires Docker with Zero28 cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=zero28 shell

# Inside container: build all platform components
cd /root/workspace/zero28
make

# This builds:
# - bl_enable (backlight enable utility)
# - bl_disable (backlight disable utility)
# - show.elf (boot splash display)
# - keymon (button monitoring daemon)
# - libmsettings.so (settings library)
# - DinguxCommander-sdl2 (file manager)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **DinguxCommander-sdl2**: `github.com/shauninman/DinguxCommander-sdl2.git` (SDL2 fork of file manager)

Note: The Zero28 uses SDL2 (unlike many MinUI platforms that use SDL 1.2).

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── zero28/             Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, bl_enable, bl_disable, etc.)
│   │   │   └── install.sh  Post-update installation script (no-op)
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale)
│       ├── line-*.png      Line effect overlays (2-8px)
│       ├── grid-*.png      Grid effect overlays (2-11px)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/            Update staging area
│   └── zero28/             Platform boot components
│       ├── show.elf        Splash screen display
│       ├── installing.png  Initial install splash
│       └── updating.png    Update splash
├── Roms/                   ROM files organized by system
└── MinUI.zip               Update package (if present)
```

### Boot Process

1. Device boots and runs `zero28.sh` (from `.tmp_update/`)
2. Script sets CPU governor to "performance" (600 MHz)
3. If `MinUI.zip` exists:
   - Display `installing.png` (first install) or `updating.png` (update)
   - Rename `.tmp_update` to `.tmp_update-old`
   - Extract `MinUI.zip` to SD card
   - Delete ZIP and old update directory
   - Run `.system/zero28/bin/install.sh` to complete setup
4. Launch MinUI via `.system/zero28/paks/MinUI.pak/launch.sh`
5. If launcher exits, power off device (prevents stock firmware from accessing card)

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction

## Platform-Specific Features

### Visual Effects System
The Zero28 has sophisticated support for retro display effects:

**Line Effects** (scanlines):
- Simulates CRT horizontal scanlines
- Available scales: 2px, 3px, 4px, 5px, 6px, 8px
- Opacity: 50% (alpha 128)

**Grid Effects** (pixel grids):
- Simulates LCD/CRT pixel structure
- Available scales: 2px, 3px, 4px, 5px, 6px, 8px, 11px
- Variable opacity based on scale (25-64%)
- Supports color tinting (e.g., DMG Game Boy green)

Effects are automatically scaled based on game resolution and applied as SDL texture overlays.

### Crisp Rendering Mode
Two rendering quality modes:
- **SHARPNESS_SOFT**: Bilinear scaling (smooth)
- **SHARPNESS_CRISP**: Nearest neighbor upscale (4x for small resolutions) + bilinear downscale

The crisp mode provides pixel-perfect scaling for low-resolution systems (Game Boy, GBC, NES) while maintaining smooth output at native resolution.

### Display Rotation
Automatically detects portrait displays (height > width) and applies 90-degree rotation to all rendering:
```c
SDL_GetCurrentDisplayMode(0, &mode);
if (mode.h > mode.w) rotate = 1;
```

### Brightness Control
Non-linear brightness mapping for better perceptual uniformity:
```
Level 0:   1 (near-off)
Level 1:   8
Level 2:  16
Level 3:  32
Level 4:  48
Level 5:  72
Level 6:  96
Level 7: 128
Level 8: 160
Level 9: 192
Level 10: 255 (max)
```

Applied via `/dev/disp` ioctl (`DISP_LCD_SET_BRIGHTNESS`).

### Volume Control
Complex volume system with multiple mixer controls:
1. One-time setup: Set base levels for "Headphone", "digital volume", and "Soft Volume Master"
2. Active control: Adjust "DAC volume" (0 = mute, 96-160 = audible range)
3. Separate settings: Speaker and headphone volumes stored independently
4. Auto-switch: Headphone jack detection switches between settings automatically

Volume scale: 0-20 (user) → 0 or 96-160 (raw DAC)

### WiFi Integration
WiFi status is monitored during battery polling (avoid separate daemon):
```c
char status[16];
getFile("/sys/class/net/wlan0/operstate", status, 16);
online = prefixMatch("up", status);
```

Accessible via `PLAT_isOnline()` for UI indicators.

## Included Tools

### Files.pak
DinguxCommander-based file manager (SDL2 fork) with:
- File operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

### Clock.pak
System clock/time display tool

### Input.pak
Input configuration utility

## Known Issues / Quirks

### Platform Quirks
1. **Deprecated Status**: Platform marked as deprecated - newer MinUI features may not be supported
2. **Button Swap**: A/B and X/Y mappings were swapped in first stock firmware release
3. **Inverted Volume**: Volume scale is inverted (63 = mute, 0 = max) unlike most platforms
4. **Joystick-Only**: No SDL keyboard support - all input via joystick API
5. **HAT D-Pad**: D-pad uses HAT events instead of discrete button events
6. **Power-Off on Exit**: Boot script powers off if MinUI exits (prevents stock firmware access)

### Development Notes
1. **SDL2 Platform**: Uses SDL2 (not SDL 1.2 like many MinUI platforms)
2. **Hardware Rendering**: Leverages SDL2 hardware-accelerated renderer with texture targets
3. **Effect System**: Sophisticated overlay system with multiple effect types and scales
4. **Dual Input Devices**: Monitors two separate `/dev/input/event*` devices for complete input
5. **Rotation Support**: Auto-detects and handles portrait displays transparently
6. **AXP2202 PMIC**: Battery monitoring via AXP2202-specific sysfs paths

### Input Limitations
- HAT D-Pad means different input handling than button-based D-pads
- Joystick API required - cannot use simpler keyboard-based input
- Button swap issue may cause confusion for users familiar with stock firmware

### Volume Quirks
- Inverted raw scale (0 = max, 63 = mute) is opposite of most platforms
- Complex mixer setup requires multiple controls configured at startup
- Uses `amixer` system calls instead of direct ALSA API (easier but less efficient)

### Backlight Quirks
- External utilities (`bl_enable`/`bl_disable`) required for backlight control
- Must use ioctl to `/dev/disp` device (not standard sysfs backlight interface)
- Separate brightness control via different ioctl command

## Testing

When testing changes:
1. Verify boot splash displays correctly (check rotation if using portrait display)
2. Test volume control with PLUS/MINUS buttons
3. Verify brightness control with MENU+PLUS/MINUS
4. Check headphone jack detection and volume switching
5. Test visual effects (grid/line overlays) with different games
6. Verify crisp vs soft rendering modes
7. Test both portrait and landscape display orientations
8. Confirm battery and WiFi status indicators work

## Deprecation Notes

This platform is marked as **deprecated** in the main MinUI distribution. Consider:
- Limited or no support for new MinUI features
- May be removed in future releases
- Users should migrate to actively supported platforms
- Bug fixes may not be prioritized

The deprecation is likely due to:
- Discontinued hardware availability
- Low user base compared to other platforms
- MagicX releasing newer devices with better support
- Complexity of inverted volume scale and button swap issues

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Platform implementation: `./platform/platform.c` (883 lines of SDL2 rendering and effects)

## Maintainer Notes

This platform represents several **unique characteristics** in MinUI:
- **SDL2 adoption**: One of few platforms using SDL2 instead of SDL 1.2
- **Hardware rendering**: Leverages GPU-accelerated rendering with texture targets
- **Effects system**: Most sophisticated visual effects implementation in MinUI
- **Joystick-centric**: Pure joystick input (no keyboard fallback)
- **AXP2202 PMIC**: Different battery monitoring IC than most platforms
- **Display rotation**: Automatic portrait mode detection and rotation

While deprecated, it demonstrates MinUI's flexibility in adapting to:
- Modern SDL2 graphics stack
- Joystick-based input systems
- Advanced rendering features (effects, crisp mode, rotation)
- Alternative PMIC hardware

This platform may serve as reference for future SDL2-based platforms or devices requiring display rotation support.
