# M17

Platform implementation for the M17 retro handheld device.

> [!WARNING]
> **This platform is deprecated and will be removed in a future MinUI release.**
>
> **Reason**: Old/weak Cortex-A7 chipset, ultra-budget outlier with limited performance.
>
> While the platform will continue to work with current MinUI releases, it will not receive new features or platform-specific bug fixes.

## Hardware Specifications

### Display
- **Resolution**: 480x273 (16:9 widescreen)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 1x (uses `assets.png`)
- **Aspect Ratio**: 16:9 widescreen format

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y (labels swapped: physical A=B, physical B=A, physical X=Y, physical Y=X)
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - SELECT button
  - START button
  - Two MENU buttons (primary + alternate)

### Input Method
- **Primary**: Hybrid input (SDL keyboard + joystick)
- **SDL Keyboard**: Face/shoulder buttons and D-pad
- **Joystick**: All buttons duplicated as joystick indices
- **Evdev Codes**: Not used (all CODE_* values are CODE_NA)

### CPU & Performance
- ARM processor with potential NEON SIMD support (HAS_NEON defined)
- Supports CPU affinity pinning (taskset 8 for core 3)
- No overclocking utilities included

### Power Management
- Headphone jack detection via sysfs (`/sys/devices/virtual/switch/h2w/state`)
- Auto audio routing for headphones vs speakers
- Basic sleep/wake functionality via MENU button

### Storage
- SD card mounted at `/sdcard`

## Directory Structure

```
m17/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, paths
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control + headphone monitoring
├── libmsettings/      Settings library (volume, brightness, jack)
├── boot/              Boot script and splash images
│   ├── boot.sh        Boot/update handler (becomes em_ui.sh)
│   ├── build.sh       Script to package boot assets
│   ├── installing.bmp 32-bit BMP boot splash (480x272)
│   ├── updating.bmp   32-bit BMP update splash (480x272)
│   └── notes.txt      Technical notes on framebuffer and boot logo
├── install/           Installation assets
│   └── install.sh     Post-update installation script
└── cores/             Libretro cores (submodules + builds)
```

## Input System

The M17 uses a **hybrid input approach**:

1. **SDL Keyboard Events**: Primary method for UI and games
2. **SDL Joystick Events**: All buttons have duplicate joystick indices
3. **No Evdev Codes**: Platform does not use kernel input codes

### Button Combinations

| Combination | Function |
|-------------|----------|
| START + R1 | Increase brightness |
| START + L1 | Decrease brightness |
| SELECT + R1 | Increase volume |
| SELECT + L1 | Decrease volume |
| MENU | Sleep device |
| MENU | Wake device |
| X (in launcher) | Resume from save state |

### Button Mapping Notes

The M17 has **swapped button labels** compared to typical layouts:
- Physical A button → Maps to SDLK_B (BUTTON_A)
- Physical B button → Maps to SDLK_A (BUTTON_B)
- Physical X button → Maps to SDLK_Y (BUTTON_X)
- Physical Y button → Maps to SDLK_X (BUTTON_Y)

This ensures consistent behavior across different devices despite different physical labeling.

### Joystick Indices

All buttons are accessible via SDL joystick API with these indices:

| Button | Joystick Index |
|--------|----------------|
| D-Pad Up | 11 |
| D-Pad Down | 14 |
| D-Pad Left | 12 |
| D-Pad Right | 13 |
| A | 9 |
| B | 4 |
| X | 2 |
| Y | 7 |
| L1 | 5 |
| R1 | 1 |
| L2 | 6 |
| R2 | 8 |
| SELECT | 10 |
| START | 3 |
| MENU | 15 |
| MENU_ALT | 16 |

## Building

### Prerequisites
Requires Docker with M17 cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=m17 shell

# Inside container: build all platform components
cd /root/workspace/m17
make

# This builds:
# - keymon (button monitoring daemon)
# - libmsettings (settings library)
# - em_ui.sh (boot script with embedded splash images)
# - All libretro cores in cores/
```

### Dependencies
The M17 platform requires two additional shared libraries from the toolchain:
- **libpng16.so.16**: PNG image library
- **libSDL2_ttf-2.0.so.0**: SDL2 TrueType font library

These are automatically extracted from the toolchain during the build process and packaged as `extra-libs.tar`.

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/sdcard/
├── .system/
│   ├── m17/                Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, etc.)
│   │   │   └── install.sh  Post-update installation script
│   │   ├── lib/            Additional libraries (extracted from extra-libs.tar)
│   │   ├── dat/            Data files (extra-libs.tar)
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets.png      UI sprite sheet (1x scale)
│       └── BPreplayBold-unhinted.otf
├── em_ui.sh                Boot script (MinUI entry point)
├── Roms/                   ROM files organized by system
├── MinUI.zip               Update package (if present)
└── log.txt                 Installation/update log (optional)
```

### Boot Process

1. Device boots and runs `/sdcard/em_ui.sh` (MinUI's boot.sh)
2. Script checks for `MinUI.zip` on SD card root
3. If ZIP found:
   - Initialize framebuffer (`/dev/fb0`)
   - Extract embedded splash images from boot script using `uudecode`
   - Display `installing` (first install) or `updating` (update) to framebuffer
   - Extract `MinUI.zip` to `/sdcard`
   - Delete ZIP file
   - Run `.system/m17/bin/install.sh` to complete setup:
     - Extract `extra-libs.tar` to `.system/m17/lib/`
     - Copy updated boot script to `/sdcard/em_ui.sh`
   - Clear framebuffer
4. Launch MinUI via taskset: `taskset 8 /sdcard/.system/m17/paks/MinUI.pak/launch.sh`
5. Loop: relaunch if launcher exits normally
6. Poweroff if launcher script is deleted (prevents stock OS from interfering)

#### Boot Script Packaging

The boot script (`em_ui.sh`) is created by a build process that:
1. Strips 54-byte headers from BMP splash images
2. Creates ZIP file containing headerless bitmaps
3. Encodes ZIP with `uuencode` and appends to `boot.sh`
4. At runtime, script extracts embedded data using line offset calculation

This allows splash images to be embedded directly in the boot script without requiring separate files.

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in `/sdcard/` root
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction
5. Updated boot script replaces `/sdcard/em_ui.sh`

## Platform-Specific Features

### Framebuffer Display

Boot splash images are displayed directly to `/dev/fb0`:
- **Resolution**: 480x272 (matches screen exactly)
- **Format**: 32-bit BMP (Windows format, flipped row order)
- **Display method**: Raw write via `dd` command after header removal
- **Initialization**: Framebuffer mode set from `/sys/class/graphics/fb0/modes`

### Headphone Jack Monitoring

A background thread in `keymon` continuously monitors headphone state:
- **Detection**: Reads `/sys/devices/virtual/switch/h2w/state` (0=speaker, 2=headphones)
- **Polling**: Checks every 1 second for state changes
- **Audio Routing**: Automatically switches between speaker and headphone volume profiles

Volume levels are maintained separately for speakers and headphones, restoring the appropriate level when switching.

### Volume and Brightness Control

**Volume**:
- Range: 0-20 (user-facing scale)
- Raw range: 0-100 (5x multiplier: `raw = value * 5`)
- Control method: ALSA amixer via `system()` call
- Command: `amixer cset name='Master Playback Volume' <percent>%`
- Separate volume levels for speakers (default: 8) and headphones (default: 4)

**Brightness**:
- Range: 0-10 (user-facing scale)
- Raw range: 0-8000 (inverted: lower values = brighter)
- Control method: GPIO PWM device (`/dev/gpio-pwm`)
- Mapping:
  - 0 → 8000 (off)
  - 5 → 5000 (mid)
  - 10 → 0 (max brightness)

### Input Event Monitoring

The keymon daemon monitors **four input devices**:
- `/dev/input/event0` through `/dev/input/event3`
- Polls at 60Hz (16.666ms interval)
- Handles button repeat with timing:
  - Initial delay: 300ms
  - Repeat interval: 100ms

### CPU Affinity

MinUI launcher runs pinned to CPU core 3:
```bash
taskset 8 /sdcard/.system/m17/paks/MinUI.pak/launch.sh
```

This may improve performance on this multi-core device by dedicating a core to the launcher.

### Hardware Volume Button Workaround

The platform includes a workaround for hardware volume buttons:
```c
case CODE_PLUS:
case CODE_MINUS:
    system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
    SetVolume(GetVolume());
    break;
```

This resets a hardware "scaled" flag and re-applies the current volume, likely to prevent conflicts between hardware volume controls and software volume management.

## UI Layout Adjustments

The M17's widescreen display uses optimized UI parameters:

| Parameter | Value | Notes |
|-----------|-------|-------|
| `MAIN_ROW_COUNT` | 7 rows | Number of visible menu rows |
| `FIXED_SCALE` | 1x | No UI scaling needed |
| Screen ratio | 16:9 | Wider aspect ratio than 4:3 platforms |

The 1x scale means the platform uses `assets.png` instead of `assets@2x.png` for UI elements.

## Boot Logo Customization

The M17 stores boot logos directly in the boot partition (`/dev/block/by-name/boot`). Technical details from notes.txt:

### Extracting Boot Logos

Different hardware revisions have different offsets:

**Rev A:**
- Horizontal logo: offset 4044800, size 31734 bytes
- Vertical logo: offset 4087296, size 31734 bytes

**Rev B:**
- Horizontal logo: offset 4045312, size 31734 bytes
- Vertical logo: offset 4087808, size 31734 bytes

Extract command:
```bash
# Rev A horizontal
dd if=/dev/block/by-name/boot of=/sdcard/logo-h.bmp bs=1 skip=4044800 count=31734
```

### Replacing Boot Logos

```bash
# Rev A horizontal (tested and working)
dd conv=notrunc if=/sdcard/logo-h.bmp of=/dev/block/by-name/boot bs=1 seek=4044800
```

### Logo Format Requirements

- **Dimensions**: 264x40 pixels (horizontal) or 40x264 (vertical, rotated)
- **Format**: 24-bit BMP, Windows format
- **Flipping**: DO NOT flip row order (opposite of runtime framebuffer images)
- **Centering**: Logo appears centered in framebuffer at 1x scale
- **Variable size**: Can use smaller dimensions (e.g., 100x100) within the size limit

**Note:** The device uses the horizontal version of the logo by default.

## Included Tools

Standard MinUI tools are available:
- **Clock.pak**: System clock/time display
- **Input.pak**: Input configuration utility

## Known Issues / Quirks

### Hardware Quirks
1. **Swapped Button Labels**: Physical button labels don't match logical mappings (A/B and X/Y swapped)
2. **Dual Menu Buttons**: Two menu button indices (15 and 16) may represent different physical buttons or modes
3. **CPU Affinity**: Launcher pinned to core 3 (taskset 8) - purpose unclear but may improve performance
4. **Volume Button Workaround**: Hardware volume buttons require special handling to reset "scaled" flag
5. **Cannot Disable Stock Volume Controls**: Hardware volume control (`gpio-volctrl.sh`) runs from read-only 500MB squashfs rootfs, making it impractical to patch or disable

### Input System Quirks
1. **Hybrid Input**: Platform defines both keyboard and joystick mappings for all buttons
2. **No Evdev**: Despite evdev codes being defined in some systems, M17 doesn't use them (all CODE_NA)
3. **Complex Joystick Indices**: Joystick button numbers are non-sequential (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **NEON Available**: May support ARM NEON SIMD (HAS_NEON defined) but uncertain
3. **Separate Volume Profiles**: Speaker and headphone volumes are tracked independently
4. **Embedded Boot Images**: Boot script includes base64-encoded ZIP file with splash images
5. **Poweroff on Exit**: Boot script powers off device if launcher is removed (safety measure)

### Boot Image Complexity
- Runtime framebuffer images: 32-bit BMP, flipped row order, 480x272
- Boot partition logos: 24-bit BMP, NOT flipped, 264x40 or smaller
- Different header offsets between hardware revisions (Rev A vs Rev B)
- Different display methods (dd to framebuffer vs. dd to boot partition)

## Testing

When testing changes:
1. Test button combinations for volume/brightness (START+L1/R1, SELECT+L1/R1)
2. Verify headphone jack detection and audio routing
3. Test both keyboard and joystick input methods
4. Check boot splash display during install/update
5. Verify CPU affinity settings (taskset 8)
6. Test repeat functionality for held buttons (300ms initial, 100ms repeat)
7. Confirm poweroff behavior when launcher exits

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Boot image notes: `./boot/notes.txt` (framebuffer and boot logo details)

## Maintainer Notes

The M17 platform demonstrates several unique characteristics:

### Unique Features
1. **Hybrid Input System**: One of few platforms defining both keyboard and joystick mappings
2. **Embedded Boot Assets**: Boot script includes uuencoded ZIP of splash images
3. **Headphone Monitoring**: Background thread for jack state detection
4. **CPU Pinning**: Uses taskset for core affinity
5. **16:9 Widescreen**: Unusual aspect ratio for retro handhelds
6. **Boot Partition Access**: Direct access to boot partition for logo customization

### Platform Complexity
- Moderate input complexity (hybrid keyboard + joystick)
- Complex boot image handling (multiple formats, embedded assets)
- Hardware revision differences (Rev A vs Rev B offsets)
- Advanced audio routing (separate speaker/headphone profiles)

### Reference Value
This platform is a good reference for:
- Implementing hybrid input systems
- Headphone jack detection and audio routing
- Embedding assets in boot scripts
- Direct framebuffer rendering
- Separate volume profiles for different audio outputs

Changes to this platform should account for the dual input methods and ensure both keyboard and joystick paths are tested.
