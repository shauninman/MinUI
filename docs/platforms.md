# MinUI Supported Platforms

This document provides detailed information about all platforms supported by MinUI.

## Platform Overview

MinUI currently supports **12+ platform families**, each representing one or more handheld gaming devices.

| Platform ID | Devices | Screen | Status |
|-------------|---------|--------|--------|
| miyoomini | Miyoo Mini, Mini Plus | 640x480 | Deprecated |
| my282 | Miyoo Mini Flip | 752x560 | Deprecated |
| my355 | Miyoo Flip, A30 | 640x480 | Deprecated |
| trimuismart | Trimui Smart, Smart Pro, Brick | 640x480 | Deprecated |
| rg35xx | RG35XX original | 640x480 | Deprecated |
| rg35xxplus | RG35XX Plus, H, 2024, RG28XX, etc. | 640x480 | Deprecated |
| rgb30 | RGB30 | 720x720 | Deprecated |
| tg5040 | Trimui Smart Pro | 1024x768 | Deprecated |
| m17 | M17 | 640x480 | Deprecated |
| gkdpixel | GKD Pixel, GKD Mini | 640x480 | Deprecated |
| magicmini | MagicX XU Mini M | 640x480 | Deprecated |
| zero28 | MagicX Mini Zero 28 | 640x480 | Deprecated |

**Note:** All platforms are marked "Deprecated" in current codebase, but remain functional.

---

## Platform Details

### Miyoo Mini Family

#### miyoomini
**Devices:** Miyoo Mini, Miyoo Mini Plus

**Specifications:**
- **Screen:** 640x480 (4:3)
- **CPU:** ARM Cortex-A7
- **RAM:** 128 MB
- **Buttons:** D-pad, A, B, X, Y, L1, R1, Select, Start, Menu
- **Features:** Battery monitoring, sleep mode

**Platform Code:** `/workspace/miyoomini/`

**Hardware Access:**
- Framebuffer: `/dev/fb0`
- Input: `/dev/input/event0`
- Backlight: `/sys/class/pwm/pwmchip0/pwm0/`
- Battery: `/sys/devices/gpiochip0/gpio/gpio59/value`

**Special Features:**
- Overclock support (up to 1.4 GHz)
- Battery percentage monitoring
- Luminance control
- ADBD support (Android Debug Bridge daemon)

**Included Utilities:**
- `show.elf` - Splash screen
- `overclock.elf` - CPU frequency control
- `batmon.elf` - Battery monitor
- `lumon.elf` - Luminance monitor
- `blank.elf` - Screen blanking

---

#### my282
**Devices:** Miyoo Mini Flip

**Specifications:**
- **Screen:** 752x560 (clamshell)
- **CPU:** ARM Cortex-A7
- **RAM:** 128 MB
- **Buttons:** D-pad, A, B, X, Y, L1, R1, Select, Start, Menu
- **Features:** Lid detection, analog stick

**Platform Code:** `/workspace/my282/`

**Hardware Access:**
- Framebuffer: `/dev/fb0`
- Input: Multiple `/dev/input/event*`
- Analog stick: Custom library
- Lid switch: GPIO

**Special Features:**
- Lid open/close detection
- Analog stick support (libmstick)
- Auto-sleep on lid close

**Included Utilities:**
- `show.elf` - Splash screen
- `libmstick.so` - Analog stick library

---

#### my355
**Devices:** Miyoo Flip, Miyoo A30

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM Cortex-A7
- **RAM:** 256 MB
- **Buttons:** Full button set
- **Features:** Enhanced hardware

**Platform Code:** `/workspace/my355/`

**Special Features:**
- Improved performance over Mini
- Better battery life

---

### Trimui Family

#### trimuismart
**Devices:** Trimui Smart, Trimui Smart Pro, Trimui Brick

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM
- **Buttons:** D-pad, A, B, X, Y, L, R, Select, Start, Menu

**Platform Code:** `/workspace/trimuismart/`

**Hardware Access:**
- Framebuffer: `/dev/fb0`
- Input: `/dev/input/event*`
- Backlight: `/sys/class/backlight/`

**Special Features:**
- Compact form factor
- Good battery life

---

#### tg5040
**Devices:** Trimui Smart Pro (high-res variant)

**Specifications:**
- **Screen:** 1024x768
- **CPU:** ARM
- **RAM:** 256 MB
- **Buttons:** Full button set
- **Features:** Higher resolution

**Platform Code:** `/workspace/tg5040/`

**Special Features:**
- High-resolution display
- Enhanced performance

---

### Anbernic Family

#### rg35xx
**Devices:** Anbernic RG35XX (original)

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM Cortex-A9
- **RAM:** 256 MB
- **Buttons:** D-pad, A, B, X, Y, L1, R1, L2, R2, Select, Start

**Platform Code:** `/workspace/rg35xx/`

**Hardware Access:**
- Framebuffer: `/dev/fb0`
- Input: Multiple event devices
- Backlight: `/sys/class/backlight/`

**Special Features:**
- Overclock support
- Good build quality
- L2/R2 triggers

**Included Utilities:**
- `overclock.elf` - CPU control
- `show.elf` - Splash screen

---

#### rg35xxplus
**Devices:** RG35XX Plus, RG35XX H, RG35XX 2024, RG28XX, RG35XXSP, RG40XXH, RG40XXV, RGCubeXX, RG34XX, RG34XXSP

**Specifications:**
- **Screen:** 640x480 (varies by model)
- **CPU:** ARM Cortex-A9
- **RAM:** 256-512 MB
- **Buttons:** Full button set, some with analog sticks
- **Features:** Enhanced hardware, HDMI output (some models)

**Platform Code:** `/workspace/rg35xxplus/`

**Hardware Access:**
- Multiple framebuffers
- Enhanced input devices
- HDMI detection (where available)

**Special Features:**
- Analog stick support (some models)
- HDMI output (some models)
- Better performance

**Included Utilities:**
- `init.elf` - System initialization
- `show.elf` - Splash screen

---

### Powkiddy

#### rgb30
**Devices:** Powkiddy RGB30

**Specifications:**
- **Screen:** 720x720 (1:1 square)
- **CPU:** ARM
- **RAM:** 256 MB
- **Buttons:** Full button set, dual analog sticks

**Platform Code:** `/workspace/rgb30/`

**Hardware Access:**
- Square framebuffer
- Dual analog sticks
- Enhanced input

**Special Features:**
- Unique square screen
- Dual analog sticks
- Vertical or horizontal orientation

---

### Other Platforms

#### m17
**Devices:** M17

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM
- **Buttons:** Standard button set

**Platform Code:** `/workspace/m17/`

---

#### gkdpixel
**Devices:** GKD Pixel, GKD Mini

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM
- **Buttons:** Standard button set
- **Features:** Compact design

**Platform Code:** `/workspace/gkdpixel/`

---

#### magicmini
**Devices:** MagicX XU Mini M

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM
- **Buttons:** Standard button set

**Platform Code:** `/workspace/magicmini/`

---

#### zero28
**Devices:** MagicX Mini Zero 28

**Specifications:**
- **Screen:** 640x480
- **CPU:** ARM
- **Buttons:** Standard button set

**Platform Code:** `/workspace/zero28/`

**Special Features:**
- Custom backlight control

**Included Utilities:**
- `bl.elf` - Backlight control
- `show.elf` - Splash screen

---

## Platform Comparison

### Screen Resolutions

| Resolution | Platforms |
|------------|-----------|
| 640x480 | miyoomini, my355, trimuismart, rg35xx, rg35xxplus, m17, gkdpixel, magicmini, zero28 |
| 752x560 | my282 |
| 720x720 | rgb30 |
| 1024x768 | tg5040 |

### Button Layouts

**Standard Layout:**
- D-pad (Up, Down, Left, Right)
- Face buttons (A, B, X, Y)
- Shoulders (L1/L, R1/R)
- System (Select, Start, Menu)

**Extended Layout (some models):**
- Extra shoulders (L2, R2)
- Analog sticks (L3, R3)
- Function button

### Features by Platform

| Feature | Platforms |
|---------|-----------|
| Battery monitoring | All |
| Sleep mode | All |
| Brightness control | All |
| Volume control | All |
| Overclock support | miyoomini, rg35xx |
| Analog sticks | my282, rgb30, some rg35xxplus |
| HDMI output | Some rg35xxplus |
| Lid detection | my282 |

---

## Platform-Specific Considerations

### miyoomini

**Performance:**
- Good performance for 8/16-bit systems
- Struggles with PS1 (use frameskip)
- Overclock recommended for demanding games

**Battery:**
- ~4-6 hours typical usage
- Battery percentage available

**Quirks:**
- Backlight control via PWM
- Custom battery detection

---

### my282

**Performance:**
- Similar to miyoomini
- Higher resolution screen

**Special Handling:**
- Lid detection requires polling
- Analog stick needs calibration
- Auto-sleep on lid close

**Quirks:**
- Clamshell form factor
- Screen protector affects display

---

### rg35xx / rg35xxplus

**Performance:**
- Better than Miyoo Mini
- Good PS1 performance
- Can handle more demanding cores

**Special Handling:**
- Multiple input devices
- Some models have HDMI
- Analog sticks (Plus models)

**Quirks:**
- Stock firmware interference
- Need to disable stock services

---

### rgb30

**Performance:**
- Good performance
- Square screen unique

**Special Handling:**
- Square framebuffer (720x720)
- Dual analog sticks
- Can rotate display

**Quirks:**
- Aspect ratio handling for rectangular content
- Letterboxing or pillarboxing always needed

---

### tg5040

**Performance:**
- High resolution
- Good performance

**Special Handling:**
- 1024x768 framebuffer
- Higher resolution assets
- More scaling options

**Quirks:**
- Larger display
- Higher power consumption

---

## Cross-Platform Development

### Platform-Independent Code

Most code should be platform-independent:
- Use `FIXED_WIDTH` / `FIXED_HEIGHT` instead of hardcoded values
- Use `SDCARD_PATH` instead of hardcoded `/mnt/SDCARD`
- Check capability defines before using features

### Platform-Specific Code

When platform-specific code is needed:

```c
#if defined(PLATFORM_MIYOOMINI)
    // Miyoo Mini specific
#elif defined(PLATFORM_RG35XX)
    // RG35XX specific
#else
    // Default/common code
#endif
```

### Capability Defines

Check capabilities instead of platforms:

```c
#ifdef HAS_HDMI
    if (PLAT_isHDMIConnected()) {
        // HDMI-specific handling
    }
#endif

#ifdef HAS_ANALOG
    int x = PAD_getAnalogX();
#endif
```

---

## Testing Recommendations

### Physical Device Testing

**Minimum testing:**
- Test on at least one physical device
- Verify basic functionality (ROM loading, save states, input)

**Thorough testing:**
- Test on multiple platform families
- Verify platform-specific features work
- Check performance on lower-end devices

### Platform Priority

**Tier 1 (Most common):**
- miyoomini (Miyoo Mini is very popular)
- rg35xxplus (Many Anbernic devices)
- trimuismart (Trimui devices)

**Tier 2:**
- rg35xx (Original RG35XX)
- my355 (Miyoo Flip/A30)
- rgb30 (Unique screen)

**Tier 3:**
- my282, tg5040, m17, gkdpixel, magicmini, zero28

---

## Platform Resources

### Toolchains

Each platform has a Docker toolchain image:
```
ghcr.io/shauninman/union-[platform]-toolchain:latest
```

### Documentation

Platform-specific notes in:
- `/workspace/[platform]/README.md` (if exists)
- `/todo.txt` (platform quirks and issues)

### Community

- GitHub Issues for bug reports
- GitHub Discussions for platform-specific questions

---

## Adding New Platforms

See [Adding Platform Support](adding-platform.md) for complete guide.

**Quick checklist:**
1. Create `/workspace/[platform]/` directory structure
2. Implement platform.c/platform.h
3. Implement libmsettings
4. Create keymon daemon
5. Add to build system
6. Test thoroughly

---

## Deprecated Platforms

### macos
**Purpose:** Development and testing on macOS

**Note:** Not intended for end users, only for development without cross-compilation.

**Platform Code:** `/workspace/macos/`

**Usage:**
```bash
cd workspace
export UNION_PLATFORM=macos
make
./all/minui/minui
```

---

## Platform Support Status

All platforms are currently marked as **deprecated** in the codebase, indicating:
- Active development has slowed
- Platforms remain functional
- Bug fixes may be limited
- Community contributions welcome

Despite deprecated status, MinUI remains the most popular custom firmware for many of these devices due to its speed, simplicity, and clean UI.
