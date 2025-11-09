# MinUI

MinUI is a focused, custom launcher and libretro frontend for [a variety of retro handhelds](#supported-devices).

<img src="github/minui-main.png" width=320 /> <img src="github/minui-menu-gbc.png" width=320 />

## Features

- Simple launcher, simple SD card
- No settings or configuration
- No boxart, themes, or distractions
- Automatically hides hidden files
  and extension and region/version
  cruft in display names
- Consistent in-emulator menu with
  quick access to save states, disc
  changing, and emulator options
- Automatically sleeps after 30 seconds
  or press POWER to sleep (and wake)
- Automatically powers off while asleep
  after two minutes or hold POWER for
  one second
- Automatically resumes right where
  you left off if powered off while
  in-game, manually or while asleep
- Resume from manually created, last
  used save state by pressing X in
  the launcher instead of A
- Streamlined emulator frontend
  (minarch + libretro cores)
- Single SD card compatible with
  multiple devices from different
  manufacturers

**[Download the latest release](https://github.com/shauninman/MinUI/releases)**

## Installation

1. Download the latest release (base ZIP for your device)
2. Extract to a freshly formatted FAT32 SD card
3. Follow device-specific instructions in the included `README.txt`
4. Insert SD card and boot your device

For detailed installation steps and device-specific setup, see the `README.txt` included in the base download.

> [!TIP]
> Devices with a physical power switch use MENU to sleep and wake instead of POWER. Once asleep, the device can be safely powered off with the switch.

## Supported Consoles

**Base consoles** (included with every release):
- Game Boy, Game Boy Color, Game Boy Advance
- Nintendo Entertainment System, Super Nintendo Entertainment System
- Sega Genesis
- PlayStation

**Extra consoles** (optional download):
- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear, Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy

## Supported Devices

MinUI currently supports **20 devices** across 4 manufacturers.

### Anbernic (H700 chipset family)

- RG35XX (December 2022)
- RG35XX Plus (November 2023)
- RG35XXH (January 2024)
- RG28XX (April 2024)
- RG35XXSP (May 2024)
- RG40XXH (July 2024)
- RG40XXV (August 2024)
- RG CubeXX (October 2024)
- RG34XX (December 2024)
- RG34XXSP (May 2025)

### Miyoo

- Mini (December 2021) - F1C100s, started the category
- Mini Plus (February 2023) - F1C200s
- A30 (May 2024) - F1C200s, Game Boy Micro style
- Mini Flip (October 2024) - F1C200s, budget clamshell
- Flip (January 2025) - RK3566, premium clamshell

### Trimui

- Smart (October 2022) - A33, 2.4" original
- Smart Pro (November 2023) - A133 Plus, 4.96" display
- Brick (October 2024) - A133 Plus, 3.2" 1024×768

### Other Manufacturers

- **Powkiddy** RGB30 (August 2023) - RK3566, 1:1 square display, community favorite
- **MagicX** Mini Zero 28 (January 2025) - A133 Plus

---

## Deprecated Devices

The following devices are **no longer actively supported** and will be removed in a future release:

- GKD Pixel (January 2024)
- M17 (October 2023)
- MagicX XU Mini M (May 2024)

> [!NOTE]
> Deprecated devices will continue to work with current MinUI releases but will not receive new features or platform-specific fixes.

---

## For Developers

Want to build MinUI, create custom paks, or understand how it works?

- **[Development Guide](docs/DEVELOPMENT.md)** - Building, testing, and contributing
- **[Architecture Guide](docs/ARCHITECTURE.md)** - How MinUI works internally
- **[Pak Development Guide](docs/PAKS.md)** - Creating custom emulator and tool paks
- **[Platform Documentation](workspace/)** - Technical hardware details for each device

## Legacy Versions

MinUI has evolved through several single-device versions before becoming the multi-device platform it is today:

- [Trimui Model S](https://github.com/shauninman/MinUI-Legacy-Trimui-Model-S) (2021/04/03 - 2021/08/06)
- [Miyoo Mini](https://github.com/shauninman/MiniUI-Legacy-Miyoo-Mini) (2022/04/20 - 2022/10/23)
- [Anbernic RG35XX](https://github.com/shauninman/MinUI-Legacy-RG35XX) (2023/02/26 - 2023/03/26)

The current multi-device MinUI launched on [September 22, 2023](https://github.com/shauninman/MinUI/releases/tag/v20230922b-2).
