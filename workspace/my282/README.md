# MY282

Platform implementation for the MY282 retro handheld device.

## Hardware Specifications

### Display
- **Resolution**: 640x480 (VGA)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1, L2, R2
- **System Buttons**:
  - MENU button
  - POWER button
  - Dedicated PLUS/MINUS volume buttons
  - SELECT and START buttons

### Input Method
- **Primary**: Joystick API (SDL joystick indices 0-18)
- **Active**: Evdev code for POWER button only (CODE_POWER = 102)
- **No SDL Keyboard**: All keyboard mappings are `BUTTON_NA`

### CPU & Performance
- ARM Cortex-A53 processor with NEON SIMD support
- NEON-FP-ARMv8 floating point unit
- Supports overclocking/underclocking via overclock utility
- Performance governor set during boot

### Power Management
- Power button detection via evdev (CODE_POWER = 102, KEY_HOME)
- System sleep/wake controlled by POWER button

### Storage
- SD card mounted at `/mnt/SDCARD`

### Audio
- Headphone jack support with automatic detection
- Separate volume levels for headphones and speakers
- Volume control via amixer

## Directory Structure

```
my282/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs, input codes
│   ├── makefile.env   Toolchain settings (Cortex-A53)
│   └── makefile.copy  File copy rules for deployment
├── keymon/            Hardware button monitoring daemon
│   ├── keymon.c       Volume/brightness control daemon
│   └── credits.txt    Attribution (based on TrimUI Smart implementation)
├── libmsettings/      Settings library
│   ├── msettings.c    Volume/brightness persistence and control
│   └── msettings.h    Settings API header
├── libmstick/         Joystick/analog stick input library
│   └── mstick.h       Analog stick API (-32768 to 32767 range)
├── show/              Boot splash screen display utility
│   └── show.c         SDL2-based image display with rotation support
├── overclock/         CPU frequency adjustment utility
│   ├── makefile       Build rules
│   └── credits.txt    Based on Steward-fu's A30 utils
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler
│   ├── update.sh      Post-update script (minimal/stub)
│   ├── installing.png 640x480 PNG boot splash
│   └── updating.png   640x480 PNG update splash
├── cores/             Libretro cores
│   ├── makefile       Copies cores from rg35xx platform
│   └── output/        Built libretro core binaries
├── other/             Third-party dependencies
│   ├── DinguxCommander-sdl2/  File manager (SDL2 version)
│   ├── unzip60/       Unzip utility for package extraction
│   └── squashfs/      Squashfs tools for filesystem management
└── tests/             Platform-specific test utilities
    ├── brightness/    Brightness control testing
    ├── calibrate/     Input calibration
    └── volume/        Volume control testing
```

## Input System

The MY282 uses a **joystick-based input architecture**:

1. **SDL Joystick API**: Primary input method with button indices 0-18
2. **Evdev Codes**: Only POWER button uses evdev (code 102)
3. **No SDL Keyboard**: Platform does not use SDL keyboard events
4. **Dual Input Devices**: Monitors `/dev/input/event0` and `/dev/input/event3`

### Joystick Button Mapping

| Button | Index | Notes |
|--------|-------|-------|
| A | 0 | Primary action button |
| B | 1 | Secondary/cancel button |
| Y | 2 | Tertiary action |
| X | 3 | Quaternary action |
| L1 | 4 | Left shoulder |
| R1 | 5 | Right shoulder |
| SELECT | 6 | Select button |
| START | 7 | Start/menu button |
| MENU | 8 | System menu |
| L2 | 9 | Left trigger |
| R2 | 10 | Right trigger |
| UP | 13 | D-pad up |
| LEFT | 14 | D-pad left |
| RIGHT | 15 | D-pad right |
| DOWN | 16 | D-pad down |
| MINUS | 17 | Volume down |
| PLUS | 18 | Volume up |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

### Input Event Quirks
- **MENU button code**: Can be 1 or 354 in kernel space (keymon handles both)
- **Volume range**: 0-20 (0 = muted)
- **Brightness range**: 0-10 (non-linear mapping)
- **Polling rate**: 60Hz (16.666ms interval)
- **Repeat delay**: 300ms initial, 100ms subsequent

## Building

### Prerequisites
Requires Docker with MY282 cross-compilation toolchain (ARM Cortex-A53).

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=my282 shell

# Inside container: build all platform components
cd /root/workspace/my282
make

# This builds:
# - keymon (button monitoring daemon)
# - libmsettings (settings library with shared memory)
# - libmstick (analog stick input library)
# - show.elf (SDL2-based splash screen display)
# - overclock.elf (CPU frequency control)
# - DinguxCommander (file manager)
# - unzip (package extraction)
# - squashfs tools
# - Copies libretro cores from rg35xx platform
```

### Toolchain Configuration

The platform uses ARM Cortex-A53 toolchain settings:
```makefile
ARCH = -marm -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -mcpu=cortex-a53
LIBS = -flto -lmstick
SDL = SDL2
```

### Dependencies

The platform automatically clones required dependencies on first build:
- **DinguxCommander-sdl2**: `github.com/shauninman/DinguxCommander-sdl2.git` (file manager)
- **unzip60**: `github.com/shauninman/unzip60.git` (archive extraction)

### Core Sharing

MY282 shares libretro cores with the rg35xx platform due to similar ARM Cortex-A53 architecture. The cores makefile simply copies pre-built binaries from `../../rg35xx/cores/output/`.

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── my282/              Platform-specific binaries
│   │   ├── bin/           Utilities (keymon, overclock, etc.)
│   │   │   └── install.sh Post-update installation script
│   │   └── paks/          Applications and emulators
│   │       └── MinUI.pak/ Main launcher
│   └── res/               Shared UI assets
│       ├── assets@2x.png  UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/           Update staging area
│   └── my282/            Platform boot components
│       ├── show.elf      Splash screen display
│       ├── unzip         Package extraction utility
│       ├── installing.png Initial install splash
│       └── updating.png  Update splash
├── .userdata/            Platform-specific user data
│   └── my282/
│       └── msettings.bin Settings persistence file
├── Roms/                 ROM files organized by system
└── MinUI.zip             Update package (if present)
```

### Boot Process

1. Device boots and runs boot script from `.tmp_update/my282.sh`
2. Script sets CPU governor to "performance" for optimal speed
3. If `MinUI.zip` exists:
   - Display `installing.png` (first install) or `updating.png` (update)
   - Move old update directory to backup location
   - Extract `MinUI.zip` to SD card using unzip utility
   - Delete ZIP file
   - Remove backup directory
   - Run `.system/my282/bin/install.sh` to complete setup
4. Launch MinUI via `.system/my282/paks/MinUI.pak/launch.sh`
5. Keep relaunching while launcher exists (infinite loop)
6. If launcher removed, execute `poweroff` to prevent stock firmware access

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root (`/mnt/SDCARD/MinUI.zip`)
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction
5. MinUI launches automatically

## Platform-Specific Features

### Settings Persistence

The platform uses a **shared memory architecture** for settings:
- Settings stored in `/dev/shm/SharedSettings` (POSIX shared memory)
- Keymon acts as "host" process managing settings
- Other processes act as "clients" reading/writing shared state
- Persisted to `/mnt/SDCARD/.userdata/my282/msettings.bin`

Settings structure:
```c
struct Settings {
    int version;      // Version for future compatibility
    int brightness;   // 0-10 (non-linear mapping to hardware)
    int headphones;   // Headphone volume (0-20)
    int speaker;      // Speaker volume (0-20)
    int jack;         // Headphone jack detection (0/1)
    int hdmi;         // HDMI output detection (0/1, unused)
};
```

### Brightness Control

Non-linear brightness mapping via `/dev/disp` ioctl:

| Level | Raw Value | Increment |
|-------|-----------|-----------|
| 0 | 3 | - |
| 1 | 4 | +1 |
| 2 | 5 | +1 |
| 3 | 6 | +1 |
| 4 | 8 | +2 |
| 5 | 12 | +4 |
| 6 | 16 | +4 |
| 7 | 24 | +8 |
| 8 | 72 | +48 |
| 9 | 128 | +56 |
| 10 | 255 | +127 |

Uses `DISP_LCD_SET_BRIGHTNESS` ioctl (0x102) on `/dev/disp`.

### Volume Control

- **Raw range**: 0-100 (mapped from 0-20 scale)
- **Control method**: amixer command (`amixer set 'headphone volume' N%`)
- **Separate levels**: Headphone vs speaker (auto-switches on jack detection)
- **Mute value**: 0

### CPU Frequency Control

The overclock utility (based on Steward-fu's A30 utils) provides CPU frequency adjustment. Unlike some platforms with direct register access, this platform uses a pre-built utility.

### Analog Stick Support

Platform includes `libmstick` library for analog stick input:
```c
void Stick_init(void);
void Stick_quit(void);
void Stick_get(int* x, int* y);  // Returns -32768 to 32767
```

This is unique compared to other MinUI platforms which typically only have D-pad input.

### Display Rotation Support

The `show.elf` utility includes rotation detection:
```c
SDL_GetCurrentDisplayMode(0, &mode);
if (mode.h > mode.w) rotate = 3;  // 270 degrees if portrait
```

This allows proper splash screen display regardless of screen orientation.

### Input Event Monitoring

Keymon daemon monitors **two separate input devices**:
- `/dev/input/event0`: Primary input events
- `/dev/input/event3`: Secondary input events

This dual-device approach ensures all hardware buttons are captured even if distributed across multiple kernel input nodes.

### Stale Input Prevention

After system sleep/resume, keymon ignores input for >1 second to prevent spurious button events:
```c
if (now - then > 1000) ignore = 1;  // Ignore stale input
```

## Included Tools

### Files.pak
DinguxCommander-SDL2 based file manager with:
- Full file operations (copy, cut, paste, delete, rename)
- Directory navigation
- SDL2 graphics rendering
- Integrated with MinUI launcher

### Clock.pak
System clock/time display tool

### Input.pak
Input configuration utility

## Known Issues / Quirks

### Hardware Quirks
1. **Dual Input Devices**: Button events split across event0 and event3
2. **MENU Code Variance**: Keymon must handle both code 1 and 354 for MENU button
3. **Shared Cores**: Uses rg35xx cores (same ARM Cortex-A53 architecture)
4. **Infinite Launch Loop**: Boot script loops indefinitely to prevent stock OS access
5. **Forced Poweroff**: Device powers off if MinUI launcher removed

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **NEON Support**: HAS_NEON defined - use SIMD optimizations for performance-critical code
3. **Joystick Input**: Unlike most MinUI platforms which use keyboard, MY282 uses joystick API
4. **Settings Architecture**: Shared memory approach is more complex than simple file-based settings
5. **60Hz Polling**: Keymon runs at 60Hz (16.666ms) for responsive button detection

### Input Limitations
- Power button (CODE_POWER = 102) only available via evdev, not SDL
- MENU button has two different kernel codes requiring special handling
- Volume buttons work through joystick indices (17/18) but also handled by keymon

### Volume/Brightness Quirks
- Brightness mapping is highly non-linear (exponential curve from level 7-10)
- Volume controlled via shell command (amixer) not direct ioctl
- Jack detection changes active volume setting (headphones vs speaker)
- HDMI detection present but unused (always returns 0)

## Testing

When testing changes:
1. Verify button input on both event0 and event3 devices
2. Test volume control with PLUS/MINUS buttons
3. Verify brightness control with MENU+PLUS/MINUS
4. Check settings persistence across reboots
5. Test headphone jack detection and volume switching
6. Verify analog stick input if used by applications
7. Confirm splash screens display correctly during install/update
8. Test boot process with and without MinUI.zip present

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)

## Maintainer Notes

The MY282 platform is notable for several unique characteristics:

### Architectural Distinctions
1. **Joystick-first input**: One of the few MinUI platforms using SDL joystick API instead of keyboard
2. **Shared memory settings**: More sophisticated than file-only persistence
3. **Dual input monitoring**: Keymon watches two event devices simultaneously
4. **Analog stick support**: Unique among handheld platforms (via libmstick)
5. **Core sharing**: Leverages rg35xx cores due to identical CPU architecture

### Code Reuse
- Toolchain settings identical to rg35xx (Cortex-A53)
- Cores copied from rg35xx without rebuilding
- Overclock utility derived from Miyoo A30 utils
- Settings library similar to other platforms but with shared memory

### Integration Points
When modifying platform code, pay attention to:
- Keymon must initialize settings as "host" process
- All other processes are settings "clients"
- Volume/brightness have separate headphone/speaker profiles
- Input code differences between SDL (joystick) and kernel (evdev)
- Non-linear brightness curve optimized for perceived brightness
- 60Hz polling rate balances responsiveness vs CPU usage

This platform demonstrates MinUI's flexibility in adapting to diverse input architectures (joystick vs keyboard) while maintaining consistent user experience across all supported devices.
