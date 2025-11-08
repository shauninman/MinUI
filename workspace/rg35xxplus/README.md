# Anbernic RG35XX Plus / H / SP

Platform implementation for the Anbernic RG35XX Plus series retro handheld devices.

## Hardware Specifications

### Display
- **Resolution**: Variable by model (runtime-detected)
  - RG35XX Plus: 640x480 (VGA)
  - RG35XX H (CubeXX): 720x720 (square)
  - RG35XX SP (RG34XX): 720x480 (widescreen)
- **HDMI Output**: 1280x720 (720p) - all variants
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Orientation**: Supports rotated framebuffer (480x640) on some variants

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - MENU button
  - POWER button (hardware code 102, KEY_HOME)
  - SELECT and START buttons
  - Dedicated PLUS/MINUS volume buttons

### Input Method
- **Primary**: Joystick API (SDL joystick indices)
- **Hardware codes**: Evdev codes for power button only (CODE_POWER = 102)
- **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### CPU & Performance
- ARM Cortex-A53 processor
- ARM NEON SIMD support
- Compiler optimizations: `-march=armv8-a+crc -mtune=cortex-a53 -mcpu=cortex-a53`
- Fast math and tree vectorization enabled

### Power Management
- Power button via evdev (KEY_HOME)
- Sleep/wake functionality
- Auto-sleep support

### Storage
- **TF1 (Internal)**: `/mnt/mmc` - Boot partition and stock OS
- **TF2 (External)**: `/mnt/sdcard` - MinUI and ROMs
- Automatic fallback: If TF2 is missing or empty, symlink to TF1

## Platform Variants

This platform supports **three hardware variants** detected at runtime:

### RG35XX Plus (Standard)
- 640x480 display
- Standard aspect ratio
- Base variant (`RGXX_MODEL` detection)
- Framebuffer: Standard or rotated (480x640)

### RG35XX H (CubeXX)
- 720x720 square display
- Unique aspect ratio for arcade/vertical games
- Detected via `RGcubexx` model string
- UI adjustments: 8 rows, 40px padding

### RG35XX SP (RG34XX)
- 720x480 widescreen display
- 3:2 aspect ratio
- Detected via `RG34xx*` model string (wildcard for variants)
- UI adjustments: Standard rows (6), standard padding (10px)

### HDMI Output Mode
- 1280x720 resolution (720p)
- Runtime detection via `/sys/class/extcon/hdmi/cable.0/state`
- UI adjustments when active: 8 rows, 40px padding
- Volume forced to 100% when HDMI connected
- Brightness control disabled during HDMI output

**Detection**: Variants are auto-detected at boot by reading `/mnt/vendor/bin/dmenu.bin` for model string. The `is_cubexx`, `is_rg34xx`, and `on_hdmi` flags configure display resolution, UI layout, and input handling accordingly.

## Directory Structure

```
rg35xxplus/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Runtime-configurable specs, variant detection flags
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control + HDMI monitoring
├── libmsettings/      Settings library (volume, brightness, HDMI)
│   └── msettings.c    Shared memory settings with variant support
├── boot/              Boot assets and scripts
│   ├── boot.sh        Main boot/update handler
│   ├── build.sh       Boot asset packaging script
│   ├── bootlogo.bmp       (640x480 - RG35XX Plus)
│   ├── bootlogo-r.bmp     (480x640 - Rotated framebuffer)
│   ├── bootlogo-s.bmp     (720x720 - RG35XX H/CubeXX)
│   ├── bootlogo-w.bmp     (720x480 - RG35XX SP)
│   ├── installing.bmp     (640x480)
│   ├── installing-r.bmp   (480x640 rotated)
│   ├── installing-s.bmp   (720x720 square)
│   ├── installing-w.bmp   (720x480 widescreen)
│   ├── updating.bmp       (640x480)
│   ├── updating-r.bmp     (480x640 rotated)
│   ├── updating-s.bmp     (720x720 square)
│   └── updating-w.bmp     (720x480 widescreen)
├── init/              Minimal SDL initialization utility
│   └── init.c         Early SDL setup (VideoMode initialization)
├── show/              Boot splash screen display
│   └── show.c         SDL2-based image display with rotation support
├── install/           Installation assets and scripts
│   └── install.sh     Post-update system configuration
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    ├── sdl2/          Custom SDL2 fork (Mali framebuffer rotation)
    ├── unzip60/       Unzip utility for update extraction
    ├── dtc/           Device Tree Compiler (for boot customization)
    └── fbset/         Framebuffer configuration utility
```

## Input System

The RG35XX Plus series uses **joystick input exclusively**:

1. **SDL Joystick API**: Primary input method for all game controls
2. **Evdev Codes**: Only power button (CODE_POWER = 102) uses direct kernel input
3. **No SDL Keyboard**: Platform does not use SDL keyboard events

### Joystick Button Mappings

| Button | Index |
|--------|-------|
| A | 0 |
| B | 1 |
| X | 3 |
| Y | 2 |
| L1 | 4 |
| R1 | 5 |
| L2 | 9 |
| R2 | 10 |
| SELECT | 6 |
| START | 7 |
| MENU | 8 |
| PLUS (Volume Up) | 18 |
| MINUS (Volume Down) | 17 |
| D-Pad Up | 13 |
| D-Pad Down | 16 |
| D-Pad Left | 14 |
| D-Pad Right | 15 |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Hardware Button Quirks
- **MENU button code**: 312 (in kernel space, also responds to 354)
- **Volume range**: 0-20
- **Brightness range**: 0-10
- **Brightness disabled**: When HDMI is active
- **Volume behavior**: Maxed to 100% when HDMI connected

## Building

### Prerequisites
Requires Docker with ARM cross-compilation toolchain (buildroot with GCC).

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=rg35xxplus shell

# Inside container: build all platform components
cd /root/workspace/rg35xxplus
make

# This builds:
# - init (early SDL initialization)
# - show.elf (boot splash display utility)
# - keymon (button monitoring daemon)
# - libmsettings (settings library)
# - Boot assets (bootlogo variants packaged into boot.sh)
# - SDL2 library (Mali framebuffer with rotation support)
# - unzip60 (update extraction)
# - dtc (device tree compiler)
# - fbset (framebuffer configuration)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **SDL2**: `github.com/JohnnyonFlame/SDL-malifbdev-rot` (Mali framebuffer with rotation)
- **unzip60**: `github.com/shauninman/unzip60.git`
- **dtc**: `github.com/dgibson/dtc` (Device Tree Compiler)
- **fbset**: `github.com/shauninman/union-fbset`

## Installation

### File System Layout

MinUI installs across two SD cards:

**TF1 (Internal) - `/mnt/mmc`**:
```
/mnt/mmc/
├── dmenu.bin              Updated from MinUI (stock menu binary)
├── .minstalled            Flag file (prevents bootlogo reinstall)
└── log.txt                Installation/boot log
```

**TF2 (External) - `/mnt/sdcard`**:
```
/mnt/sdcard/
├── .system/
│   ├── rg35xxplus/        Platform-specific binaries
│   │   ├── bin/          Utilities (keymon, init, show, etc.)
│   │   │   └── install.sh Post-update installation script
│   │   ├── dat/
│   │   │   └── dmenu.bin  Updated stock menu binary
│   │   └── paks/          Applications and emulators
│   │       └── MinUI.pak/ Main launcher
│   └── res/               Shared UI assets
│       ├── assets@2x.png  UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── .userdata/
│   └── rg35xxplus/        User settings and saves
├── Roms/                  ROM files organized by system
└── MinUI.zip              Update package (if present)
```

### Boot Process

1. Stock firmware boots from TF1 (`/mnt/mmc`)
2. Custom boot script runs (`boot.sh` embedded in bootloader)
3. Script mounts TF2 (`/mnt/sdcard`) at `/dev/mmcblk1p1`
4. If TF2 mount fails or doesn't contain MinUI: symlink `/mnt/sdcard` → `/mnt/mmc`
5. Check for `MinUI.zip` on TF2:
   - Detect device variant by reading `/mnt/vendor/bin/dmenu.bin`
   - Detect framebuffer orientation from `/sys/class/graphics/fb0/modes`
   - Select appropriate boot image suffix:
     - `-r`: Rotated framebuffer (480x640)
     - `-s`: Square display (RGcubexx)
     - `-w`: Widescreen display (RG34xx)
     - (none): Standard 640x480
   - Display `installing.bmp` (first install) or `updating.bmp` (update)
   - Extract `MinUI.zip` to `/mnt/sdcard`
   - Delete ZIP file
   - On first install: Replace stock bootlogo.bmp on boot partition (TF1)
   - Run `.system/rg35xxplus/bin/install.sh` to complete setup
6. Launch MinUI via `.system/rg35xxplus/paks/MinUI.pak/launch.sh`
7. If launcher exits, shutdown device (prevents stock firmware from accessing card)

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in TF2 root (`/mnt/sdcard/`)
2. Reboot device
3. Boot script auto-detects ZIP, determines variant, and performs update
4. ZIP is deleted after successful extraction
5. `install.sh` finalizes update (copies dmenu.bin, migrates old configs)

### Migration from RG40XXCUBE

The install script handles automatic migration from the deprecated `rg40xxcube` platform:

1. Detects old `.system/rg40xxcube` directory
2. Deletes old system folder
3. Migrates user configs from `.userdata/rg40xxcube` to `.userdata/rg35xxplus`
4. Renames config files with `-cube.cfg` suffix to preserve variant-specific settings
5. Deletes old userdata folder
6. Reboots to apply changes

This ensures users can seamlessly upgrade from the older platform naming.

## Platform-Specific Features

### Runtime Variant Detection

The platform uses **runtime configuration** based on detected hardware:

```c
// Runtime flags set during initialization
extern int is_cubexx;  // RG35XX H (720x720 square)
extern int is_rg34xx;  // RG35XX SP (720x480 widescreen)
extern int on_hdmi;    // HDMI output active

// Display resolution adapts to variant
#define FIXED_WIDTH  (is_cubexx?720:(is_rg34xx?720:640))
#define FIXED_HEIGHT (is_cubexx?720:480)

// UI layout adapts to variant/HDMI
#define MAIN_ROW_COUNT (is_cubexx||on_hdmi?8:6)
#define PADDING (is_cubexx||on_hdmi?40:10)
```

This single binary supports all three hardware variants without recompilation.

### HDMI Monitoring

The `keymon` daemon runs a background thread that monitors HDMI state:

```c
// Polls /sys/class/extcon/hdmi/cable.0/state every second
// When HDMI state changes:
// - Updates on_hdmi flag
// - Switches video routing
// - Maximizes volume (100%)
// - Disables brightness control
```

HDMI detection is automatic and handles hot-plugging during runtime.

### Multi-Resolution Boot Images

The boot process supports four image variants:

| Suffix | Resolution | Variant |
|--------|-----------|---------|
| (none) | 640x480 | RG35XX Plus (standard) |
| `-r` | 480x640 | Rotated framebuffer |
| `-s` | 720x720 | RG35XX H (CubeXX) |
| `-w` | 720x480 | RG35XX SP |

Boot script auto-detects the correct variant and displays the appropriate image.

### Framebuffer Configuration

Custom SDL2 fork supports Mali framebuffer with rotation:
- Enables ARM NEON SIMD optimizations
- Mali framebuffer driver support (`--enable-video-mali`)
- Rotation handling for portrait-mode screens
- Optimized blitting for 16-bit RGB565

### Audio Configuration

Custom audio buffer size (`SAMPLES 400`) reduces audio underruns in fceumm (NES emulator).

Volume control via ALSA:
```bash
amixer sset 'lineout volume' <percentage>
```

### Brightness Control

Direct `/dev/disp` ioctl control:
```c
#define DISP_LCD_SET_BRIGHTNESS 0x102
// Raw brightness: 0-255
// User levels: 0-10 (mapped to exponential curve)
```

### Device Tree Customization

Platform includes Device Tree Compiler (dtc) for boot-time hardware customization. This allows modification of device tree binaries for hardware configuration.

## UI Layout Adjustments

Different variants use optimized UI parameters:

| Parameter | Plus (640x480) | H/HDMI (720x720/1280x720) | SP (720x480) |
|-----------|----------------|---------------------------|--------------|
| `FIXED_WIDTH` | 640 | 720 | 720 |
| `FIXED_HEIGHT` | 480 | 720 | 480 |
| `MAIN_ROW_COUNT` | 6 rows | 8 rows | 6 rows |
| `PADDING` | 10px | 40px | 10px |
| `FIXED_SCALE` | 2x | 2x | 2x |

These adjustments maximize visible content while maintaining consistent UI appearance.

## Included Tools

Tools are provided via the shared MinUI .pak system:
- **Clock.pak**: System clock/time display
- **Input.pak**: Input configuration utility
- **Files.pak**: File manager (if included in build)

## Known Issues / Quirks

### Hardware Quirks
1. **Dual SD Card Complexity**: Requires careful TF1/TF2 management and symlink fallback
2. **Runtime Configuration**: All display parameters are runtime-calculated based on flags
3. **HDMI Audio Override**: Volume forced to 100% when HDMI connected (system limitation)
4. **Model String Detection**: Relies on stock `dmenu.bin` containing `RGcubexx` or `RG34xx` string
5. **Framebuffer Rotation**: Some units ship with rotated framebuffer (480x640) requiring special handling

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **Joystick-Only Input**: No SDL keyboard support - must use joystick API exclusively
3. **MENU Button Duality**: Kernel reports code 312 AND 354 for MENU button
4. **Platform Consolidation**: Replaces older `rg40xxcube` platform (migration handled automatically)
5. **NEON Optimizations**: Platform supports ARM NEON SIMD - use `HAS_NEON` define for optimized code paths

### Volume/Brightness Quirks
- Brightness control uses non-linear exponential curve for better perceptual response
- Volume muted at raw value 0 (simpler than other platforms)
- Both volume and brightness disabled during HDMI output
- Headphone jack monitoring defined but jack_state always returns 0 (unused feature)

### Boot Process Quirks
1. **dmenu.bin Update**: Install script copies updated `dmenu.bin` to TF1 for stock menu compatibility
2. **Bootlogo One-Time Install**: `.minstalled` flag prevents bootlogo from being reinstalled on updates
3. **Shutdown on Exit**: Boot script shutdowns device if MinUI exits (safety measure)
4. **Automatic Symlink**: If TF2 is missing/empty, automatically symlinks to TF1 for single-card operation

## Testing

When testing changes:
1. Test on **all three variants** (Plus, H/CubeXX, SP)
2. Verify each resolution: 640x480, 720x720, 720x480
3. Test rotated framebuffer variant (480x640)
4. Check HDMI output (1280x720) with hot-plug detection
5. Verify volume/brightness controls with variant-specific behavior
6. Test boot splash screens for all variants (standard, -r, -s, -w)
7. Confirm TF1/TF2 symlink fallback works when TF2 is removed
8. Test migration from `rg40xxcube` platform (if applicable)
9. Verify `dmenu.bin` update mechanism

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (runtime-configurable definitions)

## Maintainer Notes

This platform demonstrates MinUI's **advanced runtime adaptation**:
- Single binary supporting three distinct hardware variants
- Runtime display resolution and UI layout configuration
- Hot-plug HDMI detection with automatic routing
- Multi-resolution boot asset selection
- Automatic migration from deprecated platforms
- Dual SD card architecture with intelligent fallback

The consolidation of RG35XX Plus, H, and SP into a single platform (replacing the older separate `rg40xxcube` platform) significantly reduces code duplication and simplifies maintenance.

**Key Design Patterns**:
- Use of `extern int` flags for runtime configuration
- Macro definitions with ternary operators for variant-specific values
- Background thread for HDMI monitoring
- Boot-time model string detection from vendor binaries
- Shared memory settings architecture for cross-process communication

Changes to this platform should preserve runtime compatibility across all three variants and maintain HDMI hot-plug functionality.
