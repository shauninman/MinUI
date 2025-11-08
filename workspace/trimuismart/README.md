# Trimui Smart

Platform implementation for the Trimui Smart retro handheld device.

## Hardware Specifications

### Display
- **Resolution**: 320x240 (QVGA)
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 1x (uses `assets.png`)
- **Display Engine**: Allwinner Display Engine 2.0 (DE2) with hardware layer composition
- **Rotation**: Software 90-degree rotation for portrait-to-landscape conversion
- **Orientation**: Physical screen is landscape, MinUI renders portrait (then rotates)

### SoC & Architecture
- **SoC**: Allwinner F1C100s
- **CPU**: ARM926EJ-S (single core)
- **Memory Allocator**: ION (In-kernel Out-of-Nexus) for contiguous DMA buffers
- **Display Layers**: Multi-channel composition (video scaling + UI overlay)
- **NEON**: Supported (ARM NEON SIMD optimizations available)

### Input
- **D-Pad**: Up, Down, Left, Right
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1 only (no L2/R2)
- **System Buttons**:
  - MENU button
  - SELECT and START buttons
  - No dedicated POWER button accessible in platform.h
  - No PLUS/MINUS volume buttons

### Input Method
- **Primary**: SDL Keyboard events (SDLK_* codes)
- **Secondary**: Evdev codes for low-level monitoring (used by keymon)
- **No Joystick**: Platform does not use SDL joystick API

### Power Management
- **Battery Monitoring**: LRADC (Low Resolution ADC)
- **No Battery Daemon**: Unlike other platforms, Trimui Smart lacks a dedicated battery monitor overlay
- **Power Button**: Not defined in platform.h (may be handled by stock firmware)

### Storage
- SD card mounted at `/mnt/SDCARD`

## Platform Architecture

The Trimui Smart uses the Allwinner Display Engine 2.0, which provides advanced features:
- **Multi-layer composition**: Separate channels for video and UI
- **Hardware scaling**: Channel 1 supports video upscaling
- **Hardware rotation**: Used for portrait-to-landscape display conversion
- **ION memory**: Physically contiguous buffers for DMA operations
- **Zero-copy flipping**: Direct register mapping for page flips

This architecture is more complex than simple framebuffer-based platforms but enables efficient scaling and composition.

## Directory Structure

```
trimuismart/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs
│   ├── platform.c     DE2 layer management, ION allocation (1,057 lines)
│   ├── sunxi_display2.h  Allwinner DE2 ioctl definitions
│   ├── ion.h          ION memory allocator interface
│   ├── ion_sunxi.h    Allwinner-specific ION extensions
│   ├── makefile.env   Environment variables for build
│   └── makefile.copy  File installation rules
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control via button combos
├── libmsettings/      Settings library (volume, brightness, jack detection)
│   ├── msettings.c    Shared memory settings with DE2 brightness control
│   ├── msettings.h    Settings API
│   └── sunxi_display2.h  DE2 header for brightness ioctl
├── show/              Boot splash screen display
│   └── show.c         SDL-based PNG image display (rotates screen)
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler (becomes trimuismart.sh)
│   ├── installing.png Boot splash for fresh install (320x240)
│   └── updating.png   Boot splash for updates (320x240)
├── cores/             Libretro cores (submodules + builds)
│   └── makefile       12 cores: NES, GB/GBC, GBA, PSX, Genesis, SNES, etc.
└── other/             Third-party dependencies
    ├── DinguxCommander/  File manager (trimui-smart branch)
    └── unzip60/          Unzip utility for update extraction
```

## Input System

The Trimui Smart uses a **hybrid input approach** similar to Miyoo Mini:

1. **SDL Keyboard Events**: Primary input method for applications
2. **Evdev Codes**: Direct kernel input codes for low-level monitoring (used by keymon)
3. **No Joystick Input**: Platform does not use SDL joystick API

### Hardware Button Layout

| Button | SDL Keyboard Code | Evdev Code | Notes |
|--------|------------------|------------|-------|
| D-Pad Up | SDLK_UP | 103 (KEY_UP) | |
| D-Pad Down | SDLK_DOWN | 108 (KEY_DOWN) | |
| D-Pad Left | SDLK_LEFT | 105 (KEY_LEFT) | |
| D-Pad Right | SDLK_RIGHT | 106 (KEY_RIGHT) | |
| A | SDLK_SPACE | 57 (KEY_SPACE) | |
| B | SDLK_LCTRL | 29 (KEY_LEFTCTRL) | |
| X | SDLK_LSHIFT | 42 (KEY_LEFTSHIFT) | |
| Y | SDLK_LALT | 56 (KEY_LEFTALT) | |
| L1 | SDLK_TAB | 15 (KEY_TAB) | |
| R1 | SDLK_BACKSPACE | 14 (KEY_BACKSPACE) | |
| SELECT | SDLK_RCTRL | 97 (KEY_RIGHTCTRL) | |
| START | SDLK_RETURN | 28 (KEY_ENTER) | |
| MENU | SDLK_ESCAPE | 1 (KEY_ESC) | |

### Button Combinations

| Combination | Function |
|-------------|----------|
| SELECT + R1 | Increase volume |
| SELECT + L1 | Decrease volume |
| START + R1 | Increase brightness |
| START + L1 | Decrease brightness |
| X (in launcher) | Resume from save state |

### Input Device
- **Event Device**: `/dev/input/event0`
- **Polling Rate**: 60Hz (16.666ms sleep in keymon)
- **Repeat Behavior**: 300ms initial delay, then 100ms repeat interval

## Building

### Prerequisites
Requires Docker with Trimui Smart cross-compilation toolchain (Allwinner F1C100s).

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=trimuismart shell

# Inside container: build all platform components
cd /root/workspace/trimuismart
make

# This builds:
# - show.elf (boot splash display using SDL)
# - keymon (button monitoring daemon)
# - libmsettings (settings library with DE2 brightness control)
# - DinguxCommander (file manager)
# - unzip60 (update extraction utility)
# - All libretro cores in cores/
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **DinguxCommander**: `github.com/shauninman/DinguxCommander.git` (branch: `trimui-smart`)
- **unzip60**: `github.com/shauninman/unzip60.git` (custom makefile for Trimui Smart)

Note: Unlike most platforms, Trimui Smart has no SDL dependency in its platform makefile (SDL is pre-installed in stock firmware).

## Installation

### File System Layout

MinUI installs to the SD card with the following structure:

```
/mnt/SDCARD/
├── .system/
│   ├── trimuismart/          Platform-specific binaries
│   │   ├── bin/              Utilities (keymon, etc.)
│   │   │   └── install.sh    Post-update installation script
│   │   └── paks/             Applications and emulators
│   │       └── MinUI.pak/    Main launcher
│   └── res/                  Shared UI assets
│       ├── assets.png        UI sprite sheet (1x scale, 320x240)
│       └── BPreplayBold-unhinted.otf
├── .tmp_update/              Update staging area
│   └── trimuismart/          Platform boot components
│       ├── show.elf          Splash screen display
│       ├── installing.png    Initial install splash
│       ├── updating.png      Update splash
│       ├── unzip             Update extraction utility
│       └── leds_off          LED control utility
├── Roms/                     ROM files organized by system
└── MinUI.zip                 Update package (if present)
```

### Boot Process

1. Device boots and runs `trimuismart.sh` from `.tmp_update/`
2. Script sets CPU governor to "performance" mode
3. If `MinUI.zip` exists:
   - Turn off LEDs (`leds_off`)
   - Display `installing.png` (first install) or `updating.png` (update)
   - Extract `MinUI.zip` to SD card using custom `unzip` utility
   - Delete `MinUI.zip` after successful extraction
   - Run `.system/trimuismart/bin/install.sh` to complete setup
4. Launch MinUI via `.system/trimuismart/paks/MinUI.pak/launch.sh`
5. If launcher exits, poweroff device (prevents stock firmware from accessing card)

### Update Process

To update MinUI on device:
1. Place `MinUI.zip` in SD card root
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction

## Platform-Specific Features

### Display Engine 2.0 Architecture

The Trimui Smart uses Allwinner's advanced Display Engine 2.0 with multi-layer composition:

**Hardware Layers**:
- **Channel 0 (FB_CH)**: Stock framebuffer layer (disabled by MinUI)
- **Channel 1 (SCALER_CH)**: Main game display with rotation and scaling
- **Channel 2 (OVERLAY_CH)**: UI overlay with alpha blending (currently unused)

**Benefits**:
- Hardware scaling for different game resolutions
- Zero-copy page flipping via direct register access
- Independent z-ordering and blending per layer
- 90-degree rotation in hardware pipeline

### ION Memory Allocation

Uses Linux ION allocator for display buffers:
```c
// Request physically contiguous memory for DMA
ion_alloc_data.heap_id_mask = ION_HEAP_TYPE_DMA_MASK;
ion_alloc_data.flags = 0;
ion_alloc_data.len = buffer_size;
ioctl(ion_fd, ION_IOC_ALLOC, &ion_alloc_data);
```

This provides both physical addresses (for hardware DMA) and virtual addresses (for CPU access).

### Display Rotation

Software performs 90-degree rotation before hardware scaling:
```c
rotate_16bpp(src_buffer, dst_buffer, width, height);
```

**Reason**: MinUI renders in portrait orientation for UI consistency across devices, but the Trimui Smart physical screen is landscape. Rotation happens in the render pipeline.

### CPU Performance

Boot script sets CPU governor to "performance" for maximum clock speed:
```bash
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

### Settings Persistence

Uses shared memory (`/SharedSettings`) for inter-process communication:
- **Host**: keymon daemon creates and manages settings
- **Clients**: Applications map read/write access
- **Persistence**: Settings saved to `$USERDATA_PATH/msettings.bin`

Default settings:
- Brightness: 3 (0-10 scale)
- Volume (speaker): 8 (0-20 scale)
- Volume (headphones): 4 (0-20 scale)

### Brightness Control

Brightness uses Allwinner DE2 display ioctl:
```c
u32 args[4] = {0};
args[1] = brightness_value; // 0-256
ioctl(disp_fd, DISP_LCD_SET_BRIGHTNESS, args);
```

**Brightness Scale Mapping**:
| Level | Raw Value |
|-------|-----------|
| 0     | 8         |
| 1     | 12        |
| 2     | 16        |
| 3     | 24 (default) |
| 4     | 32        |
| 5     | 48        |
| 6     | 64        |
| 7     | 96        |
| 8     | 128       |
| 9     | 192       |
| 10    | 256       |

### Volume Control

Volume controlled via ALSA amixer:
```bash
amixer sset 'Lineout volume' <value>  # 0-31 (converted from 0-20 scale)
```

**Volume Features**:
- Separate levels for headphones and speakers
- Automatic jack detection support (SetJack/GetJack)
- Switches volume level when headphones plugged/unplugged
- Mute value: 0 (raw scale)

### SDL Configuration

Boot splash display uses SDL with rotation:
```c
putenv("SDL_VIDEO_FBCON_ROTATION=CCW");  // Counter-clockwise rotation
SDL_Surface *screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
```

This rotates the framebuffer output to match the physical screen orientation.

## Included Tools

### Files.pak
DinguxCommander-based file manager:
- Full file operations (copy, cut, paste, delete, rename)
- Directory navigation
- Image preview support
- Trimui Smart specific branch with optimizations

### LED Control
`leds_off` utility to disable device LEDs during boot/update process.

## Supported Libretro Cores

The Trimui Smart supports 12 libretro cores:

| Core | System | Notes |
|------|--------|-------|
| fceumm | NES/Famicom | Nintendo Entertainment System |
| gambatte | GB/GBC | Game Boy and Game Boy Color |
| gpsp | GBA | Game Boy Advance |
| pcsx_rearmed | PSX | PlayStation 1 |
| picodrive | Genesis/MD | Sega Genesis/Mega Drive, 32X, Sega CD |
| snes9x2005_plus | SNES | Super Nintendo (Blargg APU enabled) |
| mednafen_pce_fast | PC Engine | TurboGrafx-16 / PC Engine |
| mednafen_vb | Virtual Boy | Nintendo Virtual Boy |
| mednafen_supafaust | SNES | Accuracy-focused SNES core |
| mgba | GBA | Game Boy Advance (alternative core) |
| pokemini | Pokemon Mini | Pokemon Mini handheld |
| race | Neo Geo Pocket | Neo Geo Pocket / Pocket Color |

**Note**: fake-08 (PICO-8) is commented out in the cores makefile.

## Known Issues / Quirks

### Hardware Quirks
1. **No L2/R2 Buttons**: Hardware only has L1 and R1 shoulder buttons
2. **Compact Screen**: 320x240 is the smallest resolution among MinUI platforms
3. **1x UI Scale**: No scaling applied to UI assets (uses base `assets.png`)
4. **No Dedicated Power Button**: POWER button not accessible in platform.h
5. **No Battery Monitor**: Unlike other platforms, no battery overlay daemon
6. **Portrait-to-Landscape**: Requires software rotation before display

### Display Engine Complexity
1. **ION Memory Required**: Must use physically contiguous memory for DMA
2. **Multi-layer Management**: Complex channel/layer configuration compared to simple framebuffer
3. **Register Mapping**: Direct memory-mapped I/O to display registers
4. **Rotation Overhead**: Software 90-degree rotation adds processing cost

### Development Notes
1. **No L3/R3**: Platform lacks clickable analog sticks
2. **NEON Optimizations**: Platform supports ARM NEON SIMD - use `HAS_NEON` define
3. **Simple Keymon**: Simpler than other platforms (no jack detection, no HDMI, no power monitoring)
4. **Shutdown on Exit**: Boot script forces poweroff if MinUI exits (prevents stock firmware access)
5. **SDL Pre-installed**: Stock firmware provides SDL libraries (no SDL build needed)

### Input Limitations
- No L2/R2 buttons (marked as BUTTON_NA and CODE_NA)
- No analog sticks (L3/R3 not available)
- Power button not exposed in platform.h
- No dedicated volume buttons (use button combinations)

### Volume and Brightness
- Volume range: 0-20 (mapped to 0-31 for ALSA)
- Brightness range: 0-10 (mapped to 8-256 for DE2)
- Mute volume: 0 (simpler than platforms using negative values)
- No separate headphone volume control in keymon (handled by msettings library)

## Testing

When testing changes:
1. Verify boot splash displays correctly (check SDL rotation)
2. Test volume/brightness controls with button combinations
3. Verify display output on physical 320x240 screen
4. Check ION memory allocation and cleanup (no leaks)
5. Confirm 1x UI scaling displays properly (no assets@2x)
6. Test update process (ZIP extraction, install.sh execution)
7. Verify poweroff behavior when launcher exits

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
- Display engine: `./platform/platform.c` (DE2 implementation details)
- Allwinner DE2: `./platform/sunxi_display2.h` (ioctl definitions)

## Maintainer Notes

This platform represents a **compact, low-resolution** implementation in MinUI:
- Smallest screen resolution (320x240) among active platforms
- 1x UI scaling (no @2x assets)
- Advanced display architecture (Allwinner DE2) vs simple framebuffer
- ION memory allocation for DMA operations
- Software rotation for portrait-to-landscape conversion
- Simpler keymon implementation (fewer hardware features to monitor)

The Trimui Smart's Display Engine 2.0 architecture is more complex than framebuffer-based platforms but enables efficient hardware scaling and composition. This serves as a reference implementation for Allwinner SoC-based devices.

**Key Technical Characteristics**:
- F1C100s SoC (ARM926EJ-S single core)
- Multi-layer display composition
- ION memory allocator
- 90-degree software rotation
- No battery monitoring overlay
- Minimal keymon functionality

Changes to display code should account for the DE2 layer management complexity and ION memory allocation requirements.
