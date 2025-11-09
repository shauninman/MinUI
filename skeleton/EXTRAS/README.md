# MinUI Extras

Optional emulator cores and tools for MinUI. These supplement the base installation with additional systems and utilities.

**Source:** https://github.com/shauninman/minui

---

## What's Included

**Extra Emulator Cores:**
- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear
- Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy

**Platform-Specific Tools:**
- File managers
- Configuration utilities
- Platform-specific tweaks

---

## Installation

Copy the desired folders from this archive to the corresponding locations on your MinUI SD card:

- `Emus/<platform>/` → `/Emus/<platform>/` (emulator paks)
- `Tools/<platform>/` → `/Tools/<platform>/` (tool paks)

**Platform identifiers:**
- `miyoomini` - Miyoo Mini/Plus
- `my282` - Miyoo A30, RG28XX
- `my355` - Miyoo Flip, Miyoo Mini Flip
- `trimuismart` - Trimui Smart
- `tg5040` - Trimui Smart Pro, Trimui Brick
- `rg35xx` - RG35XX (original)
- `rg35xxplus` - RG35XX Plus, H, SP, 2024, Cube, RG34XX, RG40XX family
- `rgb30` - RGB30
- `zero28` - MagicX Mini Zero 28
- `m17` - M17 (deprecated)
- `gkdpixel` - GKD Pixel/Mini (deprecated)
- `magicmini` - MagicX XU Mini M (deprecated)

---

## Updating

Check the release notes to see what changed. Only copy updated paks you want to your device's specific folders.

Example: If GBA.pak was updated for miyoomini, copy just that pak to `/Emus/miyoomini/` on your SD card.

---

## BIOS Files

Some extra cores require BIOS files. Place them in `Bios/<TAG>/`:

**Required BIOS:**
- **MGBA** (GBA): `gba_bios.bin`
- **PCE** (PC Engine): `syscard3.pce`
- **PKM** (Pokémon mini): `bios.min`
- **SGB** (Super Game Boy): `sgb.bios`

**Note:** Filenames are case-sensitive. MinUI is strictly BYOB (bring your own BIOS).

---

## Platform-Specific Tools

### RGB30 Only

#### P8-NATIVE.pak and Splore.pak

Run official PICO-8 natively (faster than libretro core).

**Setup:**
1. Download PICO-8 for Raspberry Pi: https://lexaloffle.itch.io/pico-8
2. Copy the entire ZIP (e.g., `pico-8_0.2.5g_raspi.zip`) to `/Tools/rgb30/Splore.pak/`
3. Copy `P8-NATIVE.pak`, `Splore.pak`, and `Wi-Fi.pak` to `/Tools/rgb30/` and `/Emus/rgb30/` on SD card
4. Place PICO-8 carts in `/Roms/Pico-8 (P8-NATIVE)/`

**Controls:**
- **Exit P8-NATIVE**: Press START → select "SHUTDOWN"
- **Exit Splore**: Press START (in-game) → "EXIT TO SPLORE" → (in splore) START → "OPTIONS" → "SHUTDOWN PICO-8"

**Limitations:** No MinUI features (in-game menu, save states, auto-resume, sleep).

#### Wi-Fi.pak

Enable WiFi for Splore and other network features.

**Setup:**
1. Edit `wifi.txt` with your network credentials (two lines: SSID, then password):
   ```
   minui
   lessismore
   ```
2. Copy `Wi-Fi.pak` to `/Tools/rgb30/` on SD card
3. Launch pak to connect (first time) or toggle WiFi on/off (subsequent launches)

**Note:** WiFi drains battery. Enable only when needed.

---

## Device-Specific Tools

### Miyoo A30

#### Remove Loading.pak

Removes "LOADING" text between boot logo and MinUI.

**Warning:** Patches NAND memory (read-only root filesystem). Takes ~5 minutes. Stay connected to power. Do not power off during operation.

### Trimui Smart Pro / Brick

#### Remove Loading.pak

Removes "LOADING" text. Minimally invasive compared to A30 version.

### RG34XX

#### Swap Menu.pak

Swaps MENU and SELECT buttons system-wide (makes MENU more accessible).

**Usage:**
- Launch pak → reboot to apply
- Launch pak again → reboot to undo

---

## Creating Your Own Paks

Want to add more systems or tools? Check out the [Pak Development Guide](https://github.com/shauninman/minui/blob/main/docs/PAKS.md) to learn how to create custom emulator and tool paks for MinUI.

**Pak types:**
1. **Reuse existing cores** - Customize options for bundled cores
2. **Bundle new cores** - Add new libretro cores
3. **Standalone emulators** - Maximum performance (no MinUI integration)

---

## Support

**Important:** Third-party paks are not officially supported. If a console or core isn't included in MinUI's base or extras, there's usually a good reason:
- Poor libretro integration
- Unreliable save states
- Performance issues
- Complexity (arcade cores with arcane ROM requirements)

For help with MinUI itself, check the GitHub repository.
