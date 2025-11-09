# GKD Pixel

Platform implementation for the GKD Pixel retro handheld device.

> [!WARNING]
> **This platform is deprecated and will be removed in a future MinUI release.**
>
> **Reason**: Unique chipset (Ingenic X1830) with limited community support value.
>
> While the platform will continue to work with current MinUI releases, it will not receive new features or platform-specific bug fixes.

## Hardware Specifications

### Display
- **Resolution**: 320x240 (QVGA)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 1x (uses `assets.png`)
- **Video**: SDL2 with triple buffering (SDL_TRIPLEBUF)
- **Scaling**: Custom approximate bilinear scalers optimized for device resolution

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - MENU button
  - POWER button
  - SELECT and START buttons
  - Dedicated PLUS/MINUS volume buttons

### Input Method
- **Primary**: Evdev/keyboard input codes (direct kernel input)
- **No SDL Keyboard**: All SDL keyboard mappings are `BUTTON_NA`
- **No Joystick Input**: Platform does not use SDL joystick API
- **Event Device**: `/dev/input/event0` for all button input

### CPU & Performance
- ARM processor with support for GNU99 standard
- LTO (Link Time Optimization) enabled in build
- No overclocking support
- Uses GCW0 toolchain for cross-compilation

### Power Management
- Battery monitoring via sysfs (`/sys/class/power_supply/battery/capacity`)
- USB charging detection (`/sys/class/power_supply/usb/online`)
- Simplified charge levels (10%, 20%, 40%, 60%, 80%, 100%)
- Auto-sleep and power-off features

### Storage
- SD card mounted at `/media/roms`

## Architecture

The GKD Pixel uses a **pure evdev input architecture** with custom optimized scalers:

- **Evdev-only input**: All buttons mapped via kernel input event codes (no SDL keyboard or joystick)
- **Software scaling**: Uses custom approximate bilinear scalers optimized for common retro resolutions
- **Stock OS integration**: Hooks into system startup via `/usr/sbin/frontend_start`

## Directory Structure

```
gkdpixel/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs, evdev codes
│   ├── platform.c     Platform implementation (scalers, battery, etc.)
│   ├── makefile.env   Build environment settings
│   └── makefile.copy  File copy rules
├── keymon/            Hardware button monitoring daemon
│   ├── keymon.c       Volume/brightness control daemon
│   └── credits.txt    Attribution for button monitoring code
├── libmsettings/      Settings library (volume, brightness)
│   ├── msettings.h    Settings API header
│   └── msettings.c    Settings implementation (shared memory)
├── boot/              Boot and update scripts
│   ├── boot.sh        Main boot handler (becomes frontend_start)
│   ├── install.sh     Installation script
│   ├── installing.bmp Boot splash for fresh install (320x240)
│   └── updating.bmp   Boot splash for updates (320x240)
├── install/           Installation assets
│   └── install.sh     Post-update installation script
├── input/             Input testing utility
│   ├── input.c        Raw input and SDL input testing
│   └── input.txt      Input testing documentation
├── test/              Platform testing utilities
├── cores/             Libretro cores (submodules + builds)
└── makefile           Platform build orchestration
```

## Input System

The GKD Pixel uses an **evdev-only input approach**:

1. **Evdev Codes**: All input handled via direct kernel input codes (`CODE_*` defines)
2. **No SDL Input**: Platform does not use SDL keyboard or joystick APIs
3. **Single Event Device**: `/dev/input/event0` handles all button input
4. **Kernel Space Monitoring**: keymon daemon polls input device at 60Hz

### Evdev Button Mappings

| Button | Evdev Code | Linux Constant |
|--------|------------|----------------|
| UP | 103 | KEY_UP |
| DOWN | 108 | KEY_DOWN |
| LEFT | 105 | KEY_LEFT |
| RIGHT | 106 | KEY_RIGHT |
| SELECT | 1 | KEY_ESC |
| START | 28 | KEY_ENTER |
| A | 29 | KEY_LEFTCTRL |
| B | 56 | KEY_LEFTALT |
| X | 57 | KEY_SPACE |
| Y | 42 | KEY_LEFTSHIFT |
| L1 | 15 | KEY_TAB |
| R1 | 14 | KEY_BACKSPACE |
| L2 | 104 | KEY_PAGEUP |
| R2 | 109 | KEY_PAGEDOWN |
| MENU | 102 | KEY_HOME |
| MENU_ALT | 107 | KEY_END |
| POWER | 116 | KEY_POWER |
| POWEROFF | 68 | KEY_F10 |
| PLUS | 78 | KEY_KPPLUS |
| MINUS | 74 | KEY_KPMINUS |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Input Characteristics
- **Polling Rate**: 60Hz (16.666ms interval)
- **Repeat Delay**: 300ms initial, 100ms interval
- **Stale Input Protection**: Ignores events after 1+ second gap (prevents spurious events after sleep)

## Building

### Prerequisites
Requires Docker with GCW0 cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=gkdpixel shell

# Inside container: build all platform components
cd /root/workspace/gkdpixel
make

# This builds:
# - keymon (button monitoring daemon)
# - libmsettings (settings library)
# - All libretro cores in cores/
# - Platform abstraction layer (platform.c)
```

### Dependencies
- **Build Standard**: GNU99 (`-std=gnu99`)
- **Optimization**: LTO enabled (`-flto`)
- **Toolchain**: GCW0 (same as other OpenDingux devices)

No external dependencies are cloned - platform uses stock MinUI shared code.

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/media/roms/
├── .system/
│   ├── gkdpixel/          Platform-specific binaries
│   │   ├── bin/           Utilities (keymon, etc.)
│   │   │   └── install.sh Post-update installation script
│   │   ├── dat/           Platform data files
│   │   │   └── boot.sh    Boot handler script
│   │   └── paks/          Applications and emulators
│   │       └── MinUI.pak/ Main launcher
│   └── res/               Shared UI assets
│       ├── assets.png     UI sprite sheet (1x scale)
│       └── BPreplayBold-unhinted.otf
├── MinUI.zip              Update package (if present)
└── log.txt                Installation/update log
```

### Stock OS Integration

MinUI integrates with the stock firmware through system hooks:

**On system partition** (`/usr`):
```
/usr/
├── sbin/
│   ├── frontend_start          MinUI's boot.sh (copied here)
│   └── frontend_start.original Stock launcher (backed up)
└── share/
    └── minui/
        ├── installing.bmp      Fresh install splash (320x240)
        └── updating.bmp        Update splash (320x240)
```

The stock OS calls `/usr/sbin/frontend_start` on boot, which MinUI replaces with its own boot handler.

### Boot Process

1. Device boots stock OS
2. Stock OS runs `/usr/sbin/frontend_start` (MinUI's boot script)
3. Script performs console cleanup:
   - Unlocks virtual terminal
   - Resets console
   - Deactivates console on framebuffer
4. Script checks for `MinUI.zip` on `/media/roms`
5. If ZIP found:
   - Display splash to framebuffer (`installing.bmp` or `updating.bmp`)
   - Extract ZIP to `/media/roms`
   - Delete ZIP file
   - Run `.system/gkdpixel/bin/install.sh` to complete setup
6. Launch MinUI via `.system/gkdpixel/paks/MinUI.pak/launch.sh`
7. If launcher not found, fallback to stock launcher (`frontend_start.original`)

#### Boot Image Display

The platform displays BMP images directly to framebuffer:

```bash
dd skip=54 if=/usr/share/minui/installing.bmp of=/dev/fb0 bs=1
```

**Format Requirements**:
- 320x240 resolution
- Windows BMP format
- Header skip: 54 bytes (standard BMP header)
- Direct framebuffer write (no SDL required)

## Platform-Specific Features

### Framebuffer Management

The boot script performs critical framebuffer cleanup:
- **unlockvt**: Unlocks virtual terminal for framebuffer access
- **reset**: Resets console to clean state
- **vtconsole unbind**: Deactivates console text overlay on framebuffer

This ensures clean video output without console interference.

### Custom Optimized Scalers

The platform implements **custom approximate bilinear scalers** optimized for common retro resolutions:

#### GBA Scaler (240x160 → 320x213)
- **Method**: 3x3 → 4x4 pixel upscale
- **Algorithm**: Subpixel blending with color component weighting
- **Optimization**: Likely() branch prediction for identical pixel runs
- **Coverage**: Full GBA native resolution support

#### Game Boy Scaler (160x144 → 266x240)
- **Method**: 3x3 → 5x5 pixel upscale
- **Algorithm**: Subpixel blending with 2:1 and 1:2 weighting
- **Special Case**: Handles odd column remainders gracefully
- **Coverage**: Original Game Boy and Game Boy Color

#### SNES Scaler (256x224 → 320x238)
- **Method**: 4x16 → 5x17 pixel chunk upscale
- **Algorithm**: Complex multi-row blending with temporal coherence
- **Row Weighting**: 3:1 average for smooth vertical scaling
- **Coverage**: SNES, NES, and other 256px-wide systems

All scalers use:
- **RGB565 color space** (native format)
- **Magic constants** for fast bit manipulation (0xF7DE, 0x0821, 0xE79C, 0x1863)
- **Weighted averaging** to preserve visual quality
- **Branch prediction hints** (`likely()`/`unlikely()`) for identical pixel optimization

### Settings Management (libmsettings)

Uses **shared memory architecture** for settings:
- **Shared Memory Key**: `/SharedSettings`
- **Host**: keymon daemon (creates and manages settings)
- **Clients**: MinUI, emulators (read/write shared settings)
- **Persistence**: Binary file at `$USERDATA_PATH/msettings.bin`

#### Settings Schema

```c
typedef struct Settings {
    int version;      // Currently 2
    int brightness;   // 0-10
    int headphones;   // 0-20 (volume when jack detected)
    int speaker;      // 0-20 (volume when no jack)
    int unused[2];    // Reserved for future use
    int jack;         // 0-1 (runtime only, not persisted)
    int hdmi;         // 0-1 (runtime only, not persisted)
} Settings;
```

#### Hardware Control

**Brightness** (PWM via sysfs):
```
/sys/devices/platform/jz-pwm-dev.0/jz-pwm/pwm0/dutyratio
Range: 5-100 (0 prevents screen from waking)
Curve: Linear (value * 10), minimum 5
```

**Volume** (ALSA mixer):
```
amixer sset 'PCM' <percentage>
Range: 0% (mute) to 100% (max)
Transform: val ? 60 + (val * 2) / 5 : 0
Scale: 0-20 user scale → 0-100 raw scale
```

### Battery Monitoring

Simplified battery reporting via sysfs:
- **Capacity Path**: `/sys/class/power_supply/battery/capacity`
- **Charging Path**: `/sys/class/power_supply/usb/online`
- **Levels**: Reduced to 6 steps (10%, 20%, 40%, 60%, 80%, 100%)
- **Philosophy**: "Worry less about battery and more about the game you're playing"

## Included Tools

The following MinUI standard tools are available:

### Clock.pak
System clock/time display

### Input.pak
Input configuration and testing utility

## Known Issues / Quirks

### Platform Quirks

1. **No L3/R3**: Platform lacks clickable analog sticks
2. **No Rumble**: No vibration motor support
3. **No CPU Scaling**: CPU frequency adjustment not supported
4. **No VSync Control**: Fixed vsync via SDL_Delay()
5. **Small Screen**: 320x240 limits UI visibility - uses 1x assets
6. **Graphics Jitter**: Intermittent scrolling jitter in UI (not consistently reproducible, may be related to vsync timing)

### Development Notes

1. **Evdev-Only Input**: Platform completely bypasses SDL input system
   - Must poll `/dev/input/event0` directly
   - Cannot use SDL_PollEvent() for button input
   - Input testing utility (input/) demonstrates both methods

2. **Framebuffer Initialization**: Requires console cleanup before framebuffer access
   - Must call unlockvt, reset, and unbind vtconsole
   - Failure causes graphical corruption

3. **Settings Shared Memory**: First process to create `/SharedSettings` becomes host
   - keymon is expected to be the host
   - Clients must handle host process lifecycle

4. **Scaler Selection**: Platform.c selects scalers based on exact source resolution
   - 240x160: Uses optimized GBA scaler
   - 160x144: Uses optimized GB scaler
   - 256x224: Uses optimized SNES scaler
   - Other resolutions: Falls back to generic scalers (scale2x2_c16, etc.)

5. **MENU Button Alternative**: CODE_MENU_ALT (107 / KEY_END) provides alternate menu access

### Volume Quirks

- **Mute Value**: 0 (simpler than platforms using negative values)
- **Volume Range**: 0-20 (finer granularity than typical 0-10 scale)
- **Separate Profiles**: Headphone and speaker volumes stored independently
- **ALSA Transform**: Non-linear mapping to optimize useful volume range

## Testing

### Input Testing

Use the included input testing utility:

```bash
cd /root/workspace/gkdpixel/input
make
./input.elf
```

The utility provides two modes:
- **raw_input()**: Tests evdev input by reading `/dev/input/event0-3`
- **sdl_input()**: Tests SDL keyboard input (will show no events on this platform)

### When Testing Changes

1. Verify evdev input codes match kernel expectations
2. Test volume/brightness controls with MENU+PLUS/MINUS
3. Check battery monitoring displays correct charge levels
4. Verify boot splash displays correctly during install/update
5. Test all custom scalers with different resolution content:
   - GBA (240x160): Pokémon, Metroid
   - GB (160x144): Tetris, Link's Awakening
   - SNES (256x224): Super Mario World, Final Fantasy
6. Confirm framebuffer cleanup prevents console text overlay

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Platform implementation: `./platform/platform.c` (scalers, battery, rendering)

## Maintainer Notes

This platform demonstrates several unique MinUI characteristics:

### Pure Evdev Input Architecture
- **No SDL keyboard or joystick**: All input via direct kernel event codes
- **Single event device**: Simpler than platforms requiring multiple input sources
- **60Hz polling**: Direct control over input latency

### Optimized Software Scaling
- **Custom bilinear scalers**: Hand-tuned for specific retro resolutions
- **Magic constant optimization**: Fast RGB565 bit manipulation
- **Branch prediction**: Likely()/unlikely() hints for solid color regions
- **Temporal coherence**: SNES scaler maintains row state for efficiency

### Minimal Dependencies
- **No external repos**: Uses stock MinUI shared code only
- **Small footprint**: Compact platform suitable for resource-constrained device
- **GCW0 toolchain**: Leverages existing OpenDingux infrastructure

### Integration Pattern
- **Stock OS hook**: Replaces frontend_start rather than full OS replacement
- **Fallback support**: Can return to stock launcher if MinUI not found
- **Clean framebuffer**: Demonstrates proper console management

Changes to this platform should preserve the evdev-only input architecture and maintain the custom scaler optimizations that provide high-quality output on the small 320x240 display.
