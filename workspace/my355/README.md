# Miyoo Flip (MY355)

Platform implementation for the Miyoo Flip retro handheld device.

## Hardware Specifications

### Display
- **Built-in Screen**: 640x480 (VGA resolution)
- **HDMI Output**: 1280x720 (720p, runtime detection)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Rotation**: 270 degrees for built-in screen (disabled on HDMI)
- **Dynamic Layout**: Adjusts UI parameters based on active output

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2, L3, R3
- **Analog Sticks**: Left (X/Y) and Right (X/Y)
- **System Buttons**:
  - MENU button
  - POWER button (sleep/wake)
  - Dedicated PLUS/MINUS volume buttons
  - SELECT and START buttons

### Input Method
- **Primary**: SDL2 Joystick API (analog sticks supported)
- **Secondary**: Evdev codes for direct hardware access (keymon)
- **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### CPU & Performance
- ARM processor (userspace CPU frequency control)
- Performance governor with 4 speed levels:
  - **Menu**: 600 MHz (minimal power)
  - **Powersave**: 1104 MHz (battery-friendly)
  - **Normal**: 1608 MHz (default)
  - **Performance**: 1992 MHz (demanding games)

### Power Management
- Battery monitoring via sysfs (`/sys/class/power_supply/battery/capacity`)
- AC charging detection
- Backlight control with 11 brightness levels (0-10)
- Auto-sleep and power-off features

### Storage
- SD card mounted at `/mnt/SDCARD`

### Special Features
- **Hall Sensor**: Lid open/close detection for auto-sleep
- **HDMI Hotplug**: Runtime detection and automatic output switching
- **Rumble Motor**: GPIO-controlled haptic feedback (GPIO20, disabled on HDMI)
- **WiFi Status**: Network connectivity monitoring
- **LED Indicator**: Work LED for sleep state indication
- **Sharpness Modes**: Crisp (nearest-neighbor) and soft (linear) scaling
- **Overlay Effects**: Scanlines and grid overlays with color tinting
- **Headphone Jack**: Auto-detection with audio routing (GPIO150)

## Platform Architecture

The MY355 (Miyoo Flip) is unique in MinUI's platform lineup due to:

1. **Clamshell Design**: Features hall sensor for lid detection, enabling automatic sleep when closed
2. **Dual Display Support**: Seamlessly switches between built-in 640x480 screen and 720p HDMI output
3. **Advanced Video Features**: Supports multi-pass rendering for crisp scaling, overlay effects, and dynamic rotation
4. **Complex Settings System**: Uses shared memory for inter-process settings communication
5. **Special Initialization**: Requires rootfs modification via init script for first-time setup

## Directory Structure

```
my355/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs, HDMI config
│   ├── platform.c     Platform implementation (lid, HDMI, rumble, scaling)
│   └── makefile.*     Build configuration files
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control, headphone/HDMI detection
├── libmsettings/      Settings library (shared memory based)
│   ├── msettings.c    Volume, brightness, jack/HDMI state management
│   └── msettings.h    Settings API header
├── show/              Boot splash screen display utility
│   └── show.c         SDL2-based PNG image display (with rotation)
├── init/              Initial device setup (first install only)
│   └── init.sh        Rootfs modification script for MinUI integration
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler
│   ├── installing.png Boot splash for fresh install (640x480)
│   └── updating.png   Boot splash for updates (640x480)
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    ├── evtest/        Input event monitoring tool
    ├── mkbootimg/     Boot image creation tool
    ├── rsce-go/       Resource editing tool (Go)
    ├── squashfs/      Squashfs filesystem tools
    └── DinguxCommander-sdl2/ File manager dependency
```

## Input System

The MY355 uses a **hybrid input approach**:

1. **SDL2 Joystick API**: Primary input for gamepad and analog sticks
2. **Evdev Codes**: Direct kernel input codes for low-level hardware monitoring (keymon uses HID codes)
3. **No SDL Keyboard**: Platform does not use SDL keyboard events

### Evdev Code Mappings

The platform uses HID (Human Interface Device) scancodes for evdev:

| Button | HID Code | Description |
|--------|----------|-------------|
| D-Pad Up/Down/Left/Right | 82/81/80/79 | Arrow keys |
| SELECT | 228 | Left GUI (Windows key) |
| START | 40 | Enter |
| A | 44 | Space |
| B | 224 | Left Control |
| X | 225 | Left Shift |
| Y | 226 | Left Alt |
| L1 | 43 | Tab |
| R1 | 42 | Backspace |
| L2 | 75 | Page Up |
| R2 | 78 | Page Down |
| L3 | 230 | Left Alt (alt mapping) |
| R3 | 229 | Right Shift |
| MENU | 41 | Escape |
| POWER | 102 | Home |
| PLUS | 128 | Volume Up |
| MINUS | 129 | Volume Down |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

## Building

### Prerequisites
Requires Docker with MY355 cross-compilation toolchain and additional tools:
- Go compiler (for rsce-go)
- ARM cross-compiler
- Squashfs tools
- mkbootimg utilities

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=my355 shell

# Inside container: build all platform components
cd /root/workspace/my355
make

# This builds:
# - show.elf (boot splash display)
# - keymon (button monitoring daemon)
# - libmsettings.so (settings library)
# - evtest (input event debugging tool)
# - mkbootimg (boot image creation)
# - rsce_tool (resource editing)
# - squashfs tools
# - DinguxCommander (file manager)
# - All libretro cores in cores/
```

### Early Dependencies

The platform requires several external tools built first via `make early`:
- **evtest**: Input device event monitor
- **mkbootimg**: Android boot image tool
- **rsce-go**: Resource container editor (Go-based)
- **DinguxCommander-sdl2**: File manager application

These are automatically cloned and built on first run.

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── my355/              Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, etc.)
│   │   │   └── install.sh  Post-update installation script
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale)
│       ├── line-*.png      Scanline overlay effects (various scales)
│       ├── grid-*.png      Grid overlay effects (various scales)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/            Update staging area
│   └── my355/              Platform boot components
│       ├── show.elf        Splash screen display
│       ├── installing.png  Initial install splash
│       └── updating.png    Update splash
├── Roms/                   ROM files organized by system
└── MinUI.zip               Update package (if present)
```

### Initial Installation

The MY355 requires a **one-time rootfs modification** to integrate MinUI with the stock system:

1. Place installation files in `/miyoo355/app/355/` directory
2. Run `init.sh` script which:
   - Extracts current rootfs from `/dev/mtd3ro`
   - Unpacks squashfs filesystem
   - Backs up original `runmiyoo.sh` to `runmiyoo-original.sh`
   - Injects MinUI launcher hook
   - Repacks and flashes modified rootfs to `/dev/mtd3`
   - Reboots device

**Warning**: This process modifies system firmware. Backup important data first.

### Boot Process

After initialization, device boots as follows:

1. Stock system runs modified `runmiyoo.sh` boot script
2. Script sets CPU governor to "performance" mode
3. If `MinUI.zip` exists on SD card:
   - Display `installing.png` (first install) or `updating.png` (update)
   - Extract ZIP to SD card root
   - Delete ZIP file
   - Run `.system/my355/bin/install.sh` to complete setup
4. Launch MinUI via `.system/my355/paks/MinUI.pak/launch.sh`
5. Loop: if launcher exits, relaunch (prevents stock firmware access)
6. On critical failure: power off device

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root (`/mnt/SDCARD/`)
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction

## Platform-Specific Features

### Hall Sensor Lid Detection

The clamshell design includes a hall sensor for automatic lid detection:

```c
#define LID_PATH "/sys/devices/platform/hall-mh248/hallvalue"
```

- **1** = Lid open (normal operation)
- **0** = Lid closed (triggers auto-sleep)
- Checked via `PLAT_lidChanged()` during main loop
- Enables battery-saving auto-sleep when device is closed

### HDMI Hotplug Detection

HDMI connection is detected at runtime and updates video configuration:

```c
#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"
```

When HDMI is connected:
- Resolution changes from 640x480 to 1280x720
- Screen rotation disabled (HDMI always landscape)
- Rumble disabled (assumes external controller)
- Volume set to maximum (external audio system)
- UI layout adjusted: `MAIN_ROW_COUNT` increases from 6 to 8 rows
- Brightness control disabled (HDMI backlight N/A)

Detection happens on every frame flip, enabling hotplug without restart.

### Headphone Jack Detection

Automatic audio routing based on headphone connection:

```c
#define JACK_STATE_PATH "/sys/class/gpio/gpio150/value"
```

- **0** = Headphones plugged in (inverted logic)
- **1** = No headphones (use speaker)
- Monitored by keymon background thread (1 second polling)
- Automatically switches ALSA playback path: `HP` (headphones) or `SPK` (speaker)
- Separate volume settings for headphones vs speaker

### Sharpness Modes

The platform supports two scaling modes for pixel-perfect retro gaming:

**SHARPNESS_SOFT** (default):
- Linear interpolation scaling
- Smooth appearance, good for most content

**SHARPNESS_CRISP**:
- Multi-pass rendering: nearest-neighbor upscale → linear downscale
- Preserves pixel sharpness while avoiding harsh aliasing
- Adjustable hard_scale factor (1x/2x/4x) based on source resolution
- Creates intermediate texture at higher resolution for better clarity

### Overlay Effects

Support for scanline and grid overlays to simulate CRT displays:

- **EFFECT_LINE**: Horizontal scanlines (7 different scales: 2px to 8px)
- **EFFECT_GRID**: Pixel grid (6 different scales: 2px to 11px)
- Color tinting support for DMG Game Boy green effect
- Dynamic opacity adjustment based on grid size
- Automatically scaled and positioned for aspect-correct rendering

Overlay PNGs stored in `.system/res/` directory.

### Settings System

Uses **shared memory** (POSIX shm) for inter-process communication:

```c
#define SHM_KEY "/SharedSettings"
```

**Host Process** (keymon):
- Creates shared memory segment
- Loads persisted settings from `msettings.bin`
- Monitors and updates hardware state (jack, HDMI)

**Client Processes** (launcher, emulators):
- Attach to existing shared memory
- Read current settings (brightness, volume, jack/HDMI state)
- Write changes which host persists

Benefits:
- No IPC overhead for reads
- Settings survive process crashes
- Consistent state across all applications

### Brightness Scaling

Non-linear brightness curve for better perceived brightness control:

| Level | Raw Value | Perceived Brightness |
|-------|-----------|----------------------|
| 0 | 1 | Minimal (not off) |
| 1 | 6 | Very dim |
| 2 | 10 | Dim |
| 3 | 16 | Low |
| 4 | 32 | Below medium |
| 5 | 48 | Medium-low |
| 6 | 64 | Medium |
| 7 | 96 | Medium-high |
| 8 | 128 | Bright |
| 9 | 192 | Very bright |
| 10 | 255 | Maximum |

Curve follows roughly logarithmic progression to match human perception.

### Volume System

Separate volume settings for different audio outputs:

- **Speaker**: Default 8/20 (40%)
- **Headphones**: Default 4/20 (20%, quieter for ear safety)
- **HDMI**: Fixed 100/100 (maximum, delegate to external system)

Volume conversion:
```c
int raw_volume = user_volume * 5; // 0-20 → 0-100
```

Special mute handling:
- Mute sets ALSA playback path to `OFF` (not just volume 0)
- Prevents white noise on speaker mute
- Headphones still routed when muted (for cleaner audio)

### WiFi Status Monitoring

Piggybacks on battery status polling to check WiFi:

```c
getFile("/sys/class/net/wlan0/operstate", status, 16);
online = prefixMatch("up", status);
```

Displayed in UI when available, enables network-based features.

### Rumble Control

Simple GPIO-based haptic feedback:

```c
#define RUMBLE_PATH "/sys/class/gpio/gpio20/value"
```

- Binary control: 0=off, 1=on (no strength variation)
- Automatically disabled when HDMI active
- Used for in-game feedback in supported cores

### LED Indicator

Work LED indicates sleep state:

```c
#define LED_PATH "/sys/class/leds/work/brightness"
```

- **255**: Device asleep (backlight off)
- **0**: Device active (backlight on)

Provides visual feedback when lid is closed or device sleeping.

## UI Layout Adjustments

Dynamic UI parameters based on active display output:

| Parameter | Built-in Screen (640x480) | HDMI Output (1280x720) |
|-----------|---------------------------|------------------------|
| `MAIN_ROW_COUNT` | 6 rows | 8 rows |
| `PADDING` | 10px | 40px |
| Resolution | 640x480 | 1280x720 |
| Rotation | 270° | None |

These adjustments maximize visible content and maintain proper spacing on different displays.

## Included Tools

### Files.pak
DinguxCommander-SDL2 based file manager with:
- Full SDL2 support with rotation
- File operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

## Known Issues / Quirks

### Hardware Quirks
1. **Rootfs Modification Required**: First install requires system firmware modification (irreversible without backup)
2. **Clamshell Design**: Hall sensor enables lid detection but requires careful handling
3. **HDMI Hotplug**: While supported, frequent cable swapping may cause brief visual glitches
4. **Rotation Overhead**: 270-degree rotation adds minimal overhead but is always active on built-in screen
5. **Rumble Quality**: Vibration motor feels low quality (subjective)
6. **HDMI Performance**: Minarch runs at 30fps over HDMI instead of 60fps; UI padding reverts to 640x480 layout after first dirty frame

### Development Notes
1. **Go Dependency**: Build system requires Go compiler for rsce-go tool
2. **Complex Init**: Initial setup script modifies MTD flash partitions (high-risk operation)
3. **Shared Memory**: Settings system requires proper cleanup on abnormal termination
4. **Dual Input Devices**: keymon reads `/dev/input/event0` for hardware buttons
5. **NEON Support**: Platform lacks NEON SIMD (`HAS_NEON` is commented out in platform.h)

### Input Quirks
- Uses HID scancodes instead of typical Linux event codes
- Analog stick axes: LX=0, LY=1, RX=4, RY=3 (non-sequential)
- Button code 704 for MENU in keymon vs code 41 (Escape) in evdev

### Audio Quirks
- Custom sample buffer size (`SAMPLES 400`) to reduce audio underruns in fceumm
- Mute implemented via playback path change, not volume
- Always sets volume to 1% before target to ensure ALSA registers change
- **Headphone mute static**: Muting headphones produces static noise instead of silence (hardware limitation, left at 0 volume as workaround)

### Video Quirks
- Rotation applied via `SDL_RenderCopyEx` with 90-degree increments
- Crisp mode creates large intermediate textures (memory intensive for high resolutions)
- Effect overlays require exact pixel alignment for proper tiling

## Testing

When testing changes:
1. Test on **both** built-in screen and HDMI output
2. Verify lid detection and auto-sleep functionality
3. Check volume/brightness controls with button combinations
4. Test headphone jack detection and audio routing
5. Verify rumble works and disables on HDMI
6. Test WiFi status indicator accuracy
7. Validate sharpness modes (soft vs crisp) at various resolutions
8. Check overlay effects at different scale factors
9. Verify boot splash screens display correctly during install/update
10. Test settings persistence across reboots

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Platform implementation: `./platform/platform.c` (advanced features)

## Maintainer Notes

The MY355 (Miyoo Flip) represents one of the most **feature-complete** implementations in MinUI:

### Advanced Features
- Hall sensor lid detection (unique among MinUI platforms)
- Runtime HDMI hotplug with automatic reconfiguration
- Multi-pass rendering for crisp pixel scaling
- Overlay effects system (scanlines/grids)
- Shared memory settings architecture
- Dual analog stick support
- Haptic feedback (rumble)
- WiFi status monitoring

### Reference Implementation For
- Clamshell/flip device support
- Dynamic display switching (built-in screen ↔ HDMI)
- Advanced video scaling techniques
- Inter-process communication via shared memory
- Complex platform initialization (rootfs modification)

### Complexity Considerations
This platform has higher complexity than most others due to:
- Initial rootfs modification requirement
- Dual-display support with hotplug
- Shared memory IPC between processes
- Multi-pass rendering pipeline
- Runtime feature detection (lid, HDMI, headphones, WiFi)

Changes to this platform should be thoroughly tested across all feature combinations due to the complexity and interdependencies. Pay special attention to:
- HDMI hotplug behavior
- Settings persistence and shared memory cleanup
- Video pipeline performance with overlay effects
- Lid detection reliability

This platform showcases MinUI's ability to support sophisticated hardware features while maintaining the project's core philosophy of simplicity and cross-platform consistency.
