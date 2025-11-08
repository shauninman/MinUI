# Miyoo Mini / Miyoo Mini Plus

Platform implementation for the Miyoo Mini and Miyoo Mini Plus retro handheld devices.

## Hardware Specifications

### Display
- **Resolution**: 640x480 (standard) or 752x560 (Plus variant with 560p screen)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Screen Detection**: Runtime detection via `is_560p` flag

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - MENU button
  - POWER button
  - SELECT and START buttons
  - **Plus only**: Dedicated PLUS/MINUS volume buttons

### CPU & Performance
- ARM processor with NEON SIMD support
- Overclock/underclock capability via register manipulation
- Performance governor set during boot

### Power Management
- **Standard Mini**: Battery monitoring via SAR ADC
- **Plus**: Battery monitoring via AXP223 PMIC (I2C)
- Charging detection support
- Auto-sleep and power-off features

### Storage
- SD card mounted at `/mnt/SDCARD`

## Platform Variants

This platform supports two hardware variants detected at runtime:

### Standard Miyoo Mini
- 640x480 display
- SAR ADC battery monitoring
- Volume control via SELECT + L1/R1
- Brightness control via START + L1/R1

### Miyoo Mini Plus
- Optional 752x560 display (560p)
- AXP223 PMIC battery monitoring (`/customer/app/axp_test` present)
- Dedicated hardware volume buttons (PLUS/MINUS)
- Volume control also works with standalone VOLUMEUP/DOWN
- Brightness control via MENU + VOLUMEUP/DOWN or MENU + L1/R1

**Detection**: Variants are auto-detected at runtime by `keymon`. The `is_plus` and `is_560p` flags configure UI layout and input handling accordingly.

## Directory Structure

```
miyoomini/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, variant flags
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Complex daemon with dual-variant support
├── batmon/            Battery status overlay (uses AXP223 on Plus)
├── lumon/             Screen backlight/brightness control
├── blank/             Screen blanking utility
├── show/              Boot splash screen display
├── overclock/         CPU frequency adjustment via register access
├── libmsettings/      Settings library (volume, brightness, etc.)
├── install/           Installation assets and boot script
│   ├── boot.sh        Initial boot/update handler
│   ├── installing.png Boot splash for fresh install (640x480)
│   └── updating.png   Boot splash for updates (640x480)
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    ├── sdl/           Custom SDL 1.2 fork (miniui-miyoomini branch)
    └── DinguxCommander/ File manager dependency
```

## Input System

The Miyoo Mini uses a **hybrid input approach**:

1. **SDL Keyboard Events**: Primary input method for most applications
2. **Evdev Codes**: Direct kernel input codes for low-level monitoring (used by `keymon`)
3. **No Joystick Input**: Platform does not use SDL joystick API

### Button Combinations

| Combination | Function | Model |
|-------------|----------|-------|
| SELECT + L1/R1 | Volume control | Standard only |
| START + L1/R1 | Brightness control | Standard only |
| VOLUMEUP/DOWN | Volume control | Plus only |
| MENU + VOLUMEUP/DOWN | Brightness control | Plus only |
| MENU + L1/R1 | Brightness control | Plus only |
| MENU + POWER | System shutdown | Both |
| X (in launcher) | Resume from save state | Both |
| POWER | Sleep/wake device | Both |

## Building

### Prerequisites
Requires Docker with Miyoo Mini cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=miyoomini shell

# Inside container: build all platform components
cd /root/workspace/miyoomini
make

# This builds:
# - blank.elf (screen blanking)
# - batmon.elf (battery monitor overlay)
# - lumon.elf (backlight control)
# - show.elf (boot splash display)
# - overclock.elf (CPU frequency control)
# - SDL library (custom fork)
# - DinguxCommander (file manager)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **SDL 1.2**: `github.com/shauninman/SDL-1.2.git` (branch: `miniui-miyoomini`)
- **DinguxCommander**: `github.com/shauninman/DinguxCommander.git` (branch: `miniui-miyoomini`)

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── miyoomini/          Platform-specific binaries
│   │   ├── bin/           Utilities (batmon, lumon, overclock, etc.)
│   │   └── paks/          Applications and emulators
│   │       └── MinUI.pak/ Main launcher
│   └── res/               Shared UI assets
│       ├── assets@2x.png  UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/           Update staging area
│   └── miyoomini/         Platform boot components
│       ├── show.elf       Splash screen display
│       ├── installing.png Initial install splash
│       └── updating.png   Update splash
├── Roms/                  ROM files organized by system
└── MinUI.zip              Update package (if present)
```

### Boot Process

1. Device boots and runs `miyoomini.sh` from `.tmp_update/`
2. Script sets CPU governor to "performance"
3. If `MinUI.zip` exists:
   - Initialize backlight (PWM)
   - Initialize LCD (trigger `/proc/ls`)
   - Display `installing.png` (first install) or `updating.png` (update)
   - Extract `MinUI.zip` to SD card
   - Run `.system/miyoomini/bin/install.sh` to complete setup
4. Launch MinUI via `.system/miyoomini/paks/MinUI.pak/launch.sh`
5. If launcher exits, reboot (prevents stock firmware from accessing card)

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction

## Platform-Specific Features

### LCD Initialization
The platform requires a quirk to initialize the LCD:
```bash
cat /proc/ls  # Triggers LCD initialization
sleep 1
export LCD_INIT=1
```

### Backlight Control
Hardware PWM control via sysfs:
```
/sys/class/pwm/pwmchip0/export
/sys/class/pwm/pwmchip0/pwm0/period = 800
/sys/class/pwm/pwmchip0/pwm0/duty_cycle = 0-100
```

### CPU Frequency
Direct register access to MPLL (Memory PLL):
- Base address: `0x1F000000` (RIU register bank)
- MPLL offset: `+0x103000*2`
- Allows overclock/underclock adjustments

### Battery Monitoring
Two different methods based on variant:
- **Standard**: SAR ADC readings
- **Plus**: I2C communication with AXP223 PMIC at address `0x34` on `/dev/i2c-1`

## UI Layout Adjustments

The 560p variant uses different UI parameters:

| Parameter | Standard (640x480) | 560p (752x560) |
|-----------|-------------------|----------------|
| `MAIN_ROW_COUNT` | 6 rows | 8 rows |
| `PADDING` | 10px | 5px |
| `PAGE_SCALE` | 3 | 2 (memory optimization) |

These adjustments maximize visible content on the taller display while managing memory usage.

## Included Tools

### Files.pak
DinguxCommander-based file manager with:
- File operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

### Remove Loading.pak
Utility to disable the stock "loading" screen animation that appears before game launch.

## Known Issues / Quirks

### Hardware Quirks
1. **LCD Init Required**: Must read `/proc/ls` to initialize LCD on boot
2. **Variant Detection**: Plus model detected by presence of `/customer/app/axp_test`
3. **Reboot on Exit**: Boot script forces reboot if launcher exits to prevent stock firmware access

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **Hybrid Input**: Must handle both SDL keyboard and evdev codes for complete input coverage
3. **Runtime Configuration**: Many constants (resolution, button mappings) are runtime-determined based on variant flags
4. **NEON Optimizations**: Platform supports ARM NEON SIMD - use `HAS_NEON` define for optimized code paths

### Volume Quirks
- Mute volume value: `-60` (raw scale, not percentage)
- Plus model has hardware volume buttons that work independently of button combinations

## Testing

When testing changes:
1. Test on **both** standard Mini and Plus variants
2. Verify 640x480 **and** 752x560 display modes
3. Check battery monitoring on both SAR ADC and AXP223 PMIC
4. Test volume/brightness controls with variant-specific button combinations
5. Verify boot splash screens display correctly during install/update

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)

## Maintainer Notes

This is one of the most popular MinUI platforms and serves as a reference implementation for:
- Multi-variant runtime detection
- Hybrid input handling (SDL + evdev)
- Battery monitoring abstraction (ADC vs I2C)
- Display resolution flexibility

Changes to this platform should be thoroughly tested due to its wide user base and variant complexity.
