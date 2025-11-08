# Anbernic RG35XX

Platform implementation for the Anbernic RG35XX retro handheld device.

## Hardware Specifications

### Display
- **Resolution**: 640x480 (VGA)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x (uses `assets@2x.png`)
- **Framebuffer**: `/dev/fb0`

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
- **Primary**: SDL Keyboard (Japanese input key mappings)
- **Secondary**: Evdev codes for low-level hardware monitoring
- **No Joystick**: Platform does not use SDL joystick API

### Input Quirk: Japanese Key Mappings
The RG35XX hardware uses **unusual Japanese input keys** for button mappings:
- D-Pad uses: `SDLK_KATAKANA`, `SDLK_HIRAGANA`, `SDLK_HENKAN`, `SDLK_KATAKANAHIRAGANA`
- Face buttons use: `SDLK_MUHENKAN` (A), `SDLK_KP_JPCOMMA` (B), etc.

This is a hardware quirk specific to the RG35XX's button driver implementation.

### CPU & Performance
- ARM processor with NEON SIMD support
- Overclock/underclock capability via direct register access
- Voltage control via I2C PMIC interface
- CMU (Clock Management Unit) base address: `0xB0160000`

### CPU Frequency Profiles
```
1488 MHz @ 1.375V - Performance + launch (max)
1392 MHz @ 1.325V
1296 MHz @ 1.275V - Normal
1200 MHz @ 1.200V
1104 MHz @ 1.175V - Powersave
1008 MHz @ 1.100V - Anbernic default max (overvolted)
 840 MHz @ 1.075V
 720 MHz @ 1.025V
 504 MHz @ 1.000V - Menu browsing
 240 MHz @ 0.975V - Minimum
```

All frequencies are overvolted for stability compared to stock values.

### Power Management
- Battery monitoring via msettings library
- Headphone jack detection via sysfs (`/sys/class/switch/h2w/state`)
- Backlight power control (`/sys/class/backlight/backlight.2/bl_power`)
- Auto-sleep and power-off features

### Storage
- **Primary partition**: `/mnt/mmc` (ROMs partition, TF1)
- **Secondary partition**: `/mnt/sdcard` (TF2, optional)
- **SD Card Path**: `/mnt/sdcard` (with fallback symlink to `/mnt/mmc` if TF2 unavailable)

## Platform Architecture

### Dual Partition System
The RG35XX uses a flexible dual-partition approach:
- **TF1** (`/mnt/mmc`): Primary ROMS partition
- **TF2** (`/mnt/sdcard`): Optional secondary partition

Boot script detects which partition contains MinUI (`.system/rg35xx` or `MinUI.zip`) and creates symlinks accordingly. This allows MinUI to work with single or dual TF card setups.

### Boot Firmware Integration
MinUI runs through a custom ramdisk (`ramdisk.img`) and boot script (`dmenu.bin`) installed in the `/misc` partition:
- Ramdisk provides chroot environment with ext2 filesystem (`rootfs.ext2`)
- Boot script mounts SD cards and launches MinUI
- Falls back to stock `dmenu.bin` if MinUI is not installed

## Directory Structure

```
rg35xx/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, Japanese key codes
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control, headphone detection
├── libmsettings/      Settings library (volume, brightness, jack)
│   └── msettings.c    Shared memory settings with sysfs hardware access
├── overclock/         CPU frequency/voltage adjustment utility
│   └── overclock.c    Direct CMU register access + I2C PMIC control
├── boot/              Boot assets and initialization script
│   ├── boot.sh        Main boot script (becomes /misc/dmenu.bin)
│   ├── build.sh       Packages boot assets into boot.sh
│   ├── boot_logo.png  Boot splash (2.5KB PNG)
│   ├── installing.bmp Fresh install splash (640x480, 32-bit BMP)
│   └── updating.bmp   Update splash (640x480, 32-bit BMP)
├── install/           Post-update installation handler
│   └── install.sh     Updates /misc partition with new ramdisk/boot files
├── ramdisk/           Custom ramdisk assets
│   ├── patched-ramdisk.img  Modified ramdisk with custom charging screen
│   ├── charging.png   Custom charging graphic (uses /misc/charging.png)
│   └── readme.txt     Ramdisk modification instructions
├── cores/             Libretro cores (submodules + builds)
└── other/             Third-party dependencies
    └── DinguxCommander/ File manager (minui-rg35xx branch)
```

## Input System

The RG35XX uses a **hybrid dual-input approach**:

1. **SDL Keyboard Events**: Primary input method with Japanese key quirk
2. **Evdev Codes**: Direct kernel input codes for `keymon` hardware monitoring
3. **No Joystick Input**: Platform does not use SDL joystick API

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| MENU + PLUS | Increase brightness |
| MENU + MINUS | Decrease brightness |
| MENU + POWER | System shutdown |
| X (in launcher) | Resume from save state |
| POWER | Sleep/wake device |

### Input Event Devices
Keymon monitors multiple input devices:
- `/dev/input/event0`: Gamepad input
- `/dev/input/event1`: Additional input device

## Building

### Prerequisites
Requires Docker with RG35XX cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=rg35xx shell

# Inside container: build all platform components
cd /root/workspace/rg35xx
make

# This builds:
# - keymon (button monitoring daemon)
# - overclock (CPU frequency utility)
# - libmsettings (settings library)
# - boot.sh with embedded assets (via build.sh)
# - DinguxCommander (file manager)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **DinguxCommander**: `github.com/shauninman/DinguxCommander.git` (branch: `minui-rg35xx`)

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/sdcard/  (or /mnt/mmc if single partition)
├── .system/
│   ├── rg35xx/             Platform-specific binaries
│   │   ├── bin/            Utilities (keymon, overclock, install.sh)
│   │   ├── paks/           Applications and emulators
│   │   │   └── MinUI.pak/  Main launcher
│   │   ├── dat/            Boot partition files (ramdisk.img, dmenu.bin)
│   │   └── rootfs.ext2     Chroot filesystem image
│   └── res/                Shared UI assets
│       ├── assets@2x.png   UI sprite sheet (2x scale)
│       └── BPreplayBold-unhinted.otf
├── Roms/                   ROM files organized by system
└── MinUI.zip               Update package (if present)
```

### Boot Partition Layout (`/misc`)

MinUI modifies the `/misc` partition during installation:

```
/misc/
├── dmenu.bin          MinUI boot script (boot.sh)
├── ramdisk.img        Custom ramdisk with MinUI environment
├── boot_logo.bmp.gz   Boot splash (only installed, never updated)
├── charging.png       Custom charging graphic (only installed, never updated)
└── .minstalled        Flag file indicating MinUI is installed
```

### Boot Process

1. Stock firmware boots and runs `/misc/dmenu.bin` (MinUI's boot.sh)
2. Script mounts SD card partitions (TF1 at `/mnt/mmc`, TF2 at `/mnt/sdcard`)
3. Detects which partition contains MinUI by checking for `.system/rg35xx` or `MinUI.zip`
4. Creates symlink if needed to unify partition access
5. If `MinUI.zip` exists:
   - Display splash screen based on install state (installing.bmp or updating.bmp)
   - Set framebuffer to 640x480x16
   - Extract splash images from uuencoded data in boot script
   - Display appropriate splash to `/dev/fb0`
   - Extract `MinUI.zip` to SD card
   - Delete ZIP file
   - Run `.system/rg35xx/bin/install.sh` to update `/misc` partition
   - Reboot device
6. Check for `rootfs.ext2` (required for MinUI)
7. If missing, fall back to stock `dmenu.bin`
8. Setup loopback device and mount rootfs as chroot environment
9. Bind mount system directories (`/dev`, `/proc`, `/sys`, etc.)
10. `chroot` into rootfs and launch MinUI via `launch.sh`
11. On exit: unmount, detach loopback, reboot device

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. Install script checks MD5 checksums of `/misc` files
5. Only updates `/misc` if files have changed (preserves custom boot logo)
6. Device automatically reboots after update

### Install Script Behavior

The `install.sh` script is intelligent about updates:
- **First install**: Backs up original files to `/mnt/mmc/bak/`, installs all files
- **Updates**: Compares MD5 checksums before updating
- **Never overwrites**: `boot_logo.bmp.gz` or `charging.png` (user customization preserved)
- **Always updates**: `dmenu.bin` and `ramdisk.img` if changed

## Platform-Specific Features

### Brightness Control
Hardware PWM control via sysfs with 11 brightness levels (0-10):

| Level | Raw Value | Increment |
|-------|-----------|-----------|
| 0     | 16        | -         |
| 1     | 24        | 8         |
| 2     | 40        | 16        |
| 3     | 64        | 24        |
| 4     | 128       | 64        |
| 5     | 192       | 64        |
| 6     | 256       | 64        |
| 7     | 384       | 128       |
| 8     | 512       | 128       |
| 9     | 768       | 256       |
| 10    | 1024      | 256       |

Path: `/sys/class/backlight/backlight.2/brightness`

### Volume Control
Volume range: 0-20 (finer granularity than typical 0-10 scale)
- Raw range: 0-40 (multiplied by 2)
- Separate settings for speaker and headphones
- Automatic switching based on jack state
- Path: `/sys/class/volume/value`

### Headphone Jack Detection
Background thread in `keymon` polls jack state every second:
- Path: `/sys/class/switch/h2w/state`
- Automatically switches volume profile when headphones are plugged/unplugged

### CPU Frequency Control
The overclock utility provides direct CMU register manipulation:
- Register access via `/dev/mem` at `0xB0160000`
- Voltage control via I2C device `/sys/class/i2c-adapter/i2c-1/1-0065/reg_dbg`
- Always sets voltage to maximum before changing frequency (safety)
- Then adjusts to target voltage for stability

Voltage formula: `((volts - 700000) / 25000) << 7 | 0xe04e`

### Chroot Environment
MinUI runs in a chroot environment from `rootfs.ext2`:
- Loopback mounted at `/cfw` via `/dev/block/loop7`
- Provides isolated userspace with necessary libraries
- System directories bind-mounted from host (`/dev`, `/proc`, `/sys`)
- Allows MinUI to use different library versions than stock OS

### Custom Charging Screen
The ramdisk is patched to use a custom charging graphic:
- Stock charger binary modified via `sed` to always use `/misc/charging.png`
- User can replace `charging.png` for custom charging screen
- Change persists across updates (install script doesn't overwrite)

## Included Tools

### Files.pak
DinguxCommander-based file manager with:
- File operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Integrated with MinUI launcher

## Known Issues / Quirks

### Hardware Quirks
1. **Japanese Key Mappings**: SDL buttons use unusual Japanese input key codes (Katakana, Hiragana, etc.)
2. **Dual Partition Detection**: Complex boot logic to support single or dual TF card configurations
3. **Chroot Requirement**: MinUI requires `rootfs.ext2` and cannot run directly on stock firmware
4. **Reboot on Exit**: Boot script forces reboot if MinUI exits to prevent system instability

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **Evdev + SDL**: Must handle both SDL keyboard and evdev codes for complete functionality
3. **Register Access**: Overclock requires `/dev/mem` access and `mmap()` for CMU registers
4. **I2C Voltage Control**: PMIC voltage adjustment uses I2C sysfs interface (device `1-0065`)
5. **UUencoded Boot Images**: Boot script embeds splash images as uuencoded binary data
6. **Custom Ramdisk**: Ramdisk modification requires `u-boot-tools` for CRC recalculation

### Volume Quirks
- Raw volume scale (0-40) is different from user-facing scale (0-20)
- Separate volume settings for headphones vs speaker
- Mute value: 0 (raw scale)

### Boot Image Format
- Installing/updating BMPs are 640x480, 32-bit XRGB format
- Embedded in boot.sh via `uuencode` to avoid separate file dependencies
- Extracted to `/tmp` during boot and displayed directly to framebuffer

### Ramdisk Patching
Custom ramdisk cannot be created by simple hex editing due to CRC checksum:
1. Strip 64-byte header from stock ramdisk
2. Unpack with `cpio`
3. Modify `charger` binary with `sed`
4. Repack with `cpio` and `mkimage` (recreates header with correct CRC)

## Testing

When testing changes:
1. Test with **both single and dual partition** SD card configurations
2. Verify boot splash displays correctly during install and update
3. Check volume control with PLUS/MINUS buttons
4. Verify brightness control with MENU + PLUS/MINUS
5. Test headphone jack detection and automatic volume switching
6. Verify overclock utility doesn't cause instability
7. Confirm `/misc` partition updates work correctly
8. Test fallback to stock `dmenu.bin` when `rootfs.ext2` is missing

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Ramdisk notes: `./ramdisk/readme.txt` (modification instructions)

## Maintainer Notes

This platform demonstrates several **unique architectural patterns** in MinUI:

1. **Chroot-based execution**: Only platform that runs entirely in a chrooted environment
2. **Boot partition modification**: Directly modifies `/misc` partition for firmware integration
3. **Japanese key mappings**: Hardware quirk requiring unusual SDL key codes
4. **Direct register access**: Low-level CMU and PMIC manipulation for overclocking
5. **Flexible partition detection**: Sophisticated logic for single/dual TF card support
6. **Embedded boot assets**: UUencoded images in shell script to minimize dependencies

The RG35XX is a popular and actively used platform. Changes should be thoroughly tested, especially:
- Boot script logic (affects system stability)
- Overclock values (can cause hardware instability)
- `/misc` partition updates (incorrect updates can brick the device)

### Performance Considerations

- Default frequency: 1008 MHz (Anbernic stock max)
- Menu browsing: 504 MHz (power savings)
- Normal gaming: 1296 MHz
- Performance gaming: 1488 MHz
- All frequencies are **overvolted** compared to manufacturer specs for stability

The overclock utility is conservative and has been tested across many devices, but users should be warned that overclocking can reduce hardware lifespan.
