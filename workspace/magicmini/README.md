# Magic Mini (MagicX XU Mini M)

Platform implementation for the Magic Mini retro handheld device.

> **Note**: This platform is marked as deprecated in the main MinUI distribution. Support may be limited for newer features.

## Hardware Specifications

### Display
- **Resolution**: 640x480 (VGA)
- **Color Depth**: 16-bit RGB565 (runtime), 32-bit XRGB for boot images
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Orientation**: Portrait mode (boot images rotated 90 degrees)

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - MENU button
  - POWER button
  - Dedicated PLUS/MINUS volume buttons

### Input Method
- **Primary**: Joystick API (SDL joystick indices defined but currently unused)
- **Active**: Evdev codes for volume/power buttons only
- **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### CPU & Performance
- ARM processor with NEON SIMD support
- No overclocking support

### Power Management
- Basic power button detection via evdev (CODE_POWER = 116)
- No battery monitoring daemon

### Storage
- **Primary SD Card**: `/mnt/SDCARD` (internal/stock OS)
- **Secondary SD Card**: `/storage/TF2` (MinUI installation location)

## Platform Architecture

The Magic Mini uses a unique dual-SD-card setup where:
- **Internal SD** (`/mnt/SDCARD`): Contains stock OS and boot configuration
- **External SD** (`/storage/TF2`): Contains all MinUI files, ROMs, and data

This allows MinUI to coexist with the stock firmware by keeping all files on the second card.

## Directory Structure

```
magicmini/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, paths
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control via hardware buttons
├── libmsettings/      Settings library (volume, brightness)
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler (becomes autostart.sh)
│   ├── installing.bmp 32-bit BMP boot splash (640x480, rotated 90°)
│   ├── updating.bmp   32-bit BMP update splash (640x480, rotated 90°)
│   └── notes.txt      Technical notes on BMP format
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    └── 351files/      File manager tool (github.com/shauninman/351Files)
```

## Input System

The Magic Mini has a **simplified input architecture**:

1. **Joystick API**: Button indices are defined (JOY_A=0, JOY_B=1, etc.) but currently unused
2. **Evdev Codes**: Only POWER and volume buttons (PLUS/MINUS) use evdev
3. **No SDL Keyboard**: Platform does not use SDL keyboard events

### Input Event Devices
- **event2**: Gamepad/menu button input
- **event3**: Volume button input (PLUS/MINUS)

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
- **MENU button code**: 704 (in kernel space, different from standard SDL codes)
- **Volume range**: 0-20 (MUTE_VOLUME_RAW = 0)
- **Brightness range**: 0-10

## Building

### Prerequisites
Requires Docker with Magic Mini cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=magicmini shell

# Inside container: build all platform components
cd /root/workspace/magicmini
make

# This builds:
# - keymon (button monitoring daemon)
# - libmsettings (settings library)
# - 351Files (file manager tool)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **351Files**: `github.com/shauninman/351Files.git` (file manager)

Note: Unlike most platforms, Magic Mini has no SDL dependency in its makefile.

## Installation

### File System Layout

MinUI installs to the **secondary SD card** (`/storage/TF2`) with this structure:

```
/storage/TF2/
├── .system/
│   ├── magicmini/          Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, etc.)
│   │   │   └── install.sh  Post-update installation script
│   │   └── paks/           Applications and emulators
│   │       └── MinUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── Roms/                   ROM files organized by system
├── MinUI.zip               Update package (if present)
└── log.txt                 Installation/update log
```

### Stock OS Integration

MinUI integrates with the stock firmware through boot scripts:

**On internal SD** (`/mnt/SDCARD`):
```
SYSTEM.squashfs/
├── usr/
│   ├── bin/
│   │   └── autostart.sh    # Magic Mini's boot.sh gets installed here
│   └── config/
│       └── minui/
│           ├── installing.bmp  # Copied from install/installing.bmp
│           └── updating.bmp    # Copied from install/updating.bmp
```

The `autostart.sh` script runs on device boot and checks for MinUI updates on the secondary card.

### Boot Process

1. Device boots stock OS from internal SD
2. Stock OS runs `/usr/bin/autostart.sh` (MinUI's boot.sh)
3. Script checks for `MinUI.zip` on `/storage/TF2`
4. If ZIP found:
   - Display splash screen to framebuffer (`installing.bmp` or `updating.bmp`)
   - Extract ZIP to `/storage/TF2`
   - Delete ZIP file
   - Run `.system/magicmini/bin/install.sh` to complete setup
5. Launch MinUI via `.system/magicmini/paks/MinUI.pak/launch.sh`
6. If launcher exits, shutdown device (prevents stock OS from interfering)

#### Boot Image Display

The platform uses a unique BMP display method:

```bash
dd if=/usr/config/minui/installing.bmp of=/dev/fb0 bs=71 skip=1
echo 0,0 > /sys/class/graphics/fb0/pan
```

**Format Requirements**:
- 640x480 resolution
- 32-bit XRGB (X8 R8 G8 B8 format)
- Rotated 90 degrees (portrait orientation)
- Windows BMP format with flipped row order
- Uses `bs=71 skip=1` to skip 70-byte header + 1 byte for color shift correction

## Platform-Specific Features

### Dual SD Card Architecture
The Magic Mini's dual SD card setup provides:
- **Isolation**: MinUI completely separate from stock OS
- **Safety**: Stock firmware never touches MinUI data
- **Flexibility**: Can remove secondary SD to boot stock OS normally

### Direct Framebuffer Access
Boot images are written directly to `/dev/fb0` using `dd` command rather than using SDL or other graphics libraries. This allows splash screens before MinUI fully initializes.

### Audio Configuration
Custom audio buffer size (`SAMPLES 400`) to reduce audio underruns in fceumm (NES emulator).

### Volume Control
- Mute value: `0` (simpler than other platforms using negative values)
- Volume range: 0-20 (finer granularity than typical 0-10 scale)

### Input Event Monitoring
Uses **dual input devices**:
- `/dev/input/event2`: Gamepad and MENU button
- `/dev/input/event3`: Volume buttons (hardware dedicated buttons)

This split allows the volume buttons to work independently of the main gamepad input.

## Included Tools

### Files.pak
351Files-based file manager with scaled UI assets:
- Multiple resolution support (24px, 32px, 48px, 58px, 64px icons)
- Full file operations (copy, cut, paste, delete, rename)
- Image preview support
- Keyboard input for text entry

### Clock.pak
System clock/time display tool

### Input.pak
Input configuration utility

## Known Issues / Quirks

### Platform Quirks
1. **Deprecated Status**: Platform marked as deprecated - newer MinUI features may not be supported
2. **Dual SD Required**: Requires secondary SD card slot for MinUI installation
3. **Portrait Boot Images**: Boot splash BMPs must be rotated 90 degrees
4. **BMP Header Offset**: Uses `bs=71` instead of `bs=70` to correct color shifting issue
5. **Joystick API Unused**: Despite having joystick indices defined, platform doesn't currently use SDL joystick input
6. **Screen Sleep Issue**: Display doesn't fully turn off during sleep mode (backlight dims but doesn't blank)
7. **Audio Choppy**: Audio stuttering persists in some games even with 400 sample buffer fix
8. **Headphone Detection Unreliable**: GPIO `/sys/class/gpio/gpio86/value` always reads 1, making separate speaker/headphone volumes impractical (can only detect change events, not initial state)

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **Simplified Input**: No SDL keyboard support, limited evdev codes
3. **MENU Code Difference**: Kernel MENU button code (704) differs from typical SDL codes
4. **Stock OS Dependency**: Requires stock firmware to handle initial boot sequence
5. **Shutdown on Exit**: Boot script shutdowns device if MinUI exits (safety measure)

### Input Limitations
- Joystick button mappings are defined but **not currently used**
- Most input handling may rely on stock OS or alternative methods not documented in platform.h
- Only volume buttons and power button use evdev codes

## Testing

When testing changes:
1. Verify boot splash displays correctly (check 90-degree rotation, color shift)
2. Test on **secondary SD card** (`/storage/TF2`)
3. Check volume control with PLUS/MINUS buttons
4. Verify brightness control with MENU+PLUS/MINUS
5. Confirm MinUI launches after update extraction
6. Test shutdown behavior when launcher exits

## Deprecation Notes

This platform is marked as **deprecated** in the main MinUI distribution. Consider:
- Limited or no support for new MinUI features
- May be removed in future releases
- Users should migrate to actively supported platforms
- Bug fixes may not be prioritized

The deprecation is likely due to:
- Discontinued hardware availability
- Complexity of dual SD card setup
- Better alternatives available (newer MagicX variants, other devices)

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Boot image notes: `./install/notes.txt` (BMP format details)

## Maintainer Notes

This platform represents an **alternative architecture** in MinUI:
- Dual SD card installation (unique among MinUI platforms)
- Direct framebuffer rendering for boot images
- No SDL keyboard input (joystick or alternative method)
- Integration with stock firmware rather than replacement

While deprecated, it demonstrates MinUI's flexibility in adapting to different hardware constraints and installation methods. This approach may be useful reference for future platforms with similar dual-storage architectures.
