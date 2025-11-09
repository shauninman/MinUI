# MinUI

MinUI is a focused, custom launcher and libretro frontend for retro handheld devices. One SD card works across 20+ different handhelds from multiple manufacturers.

**Source:** https://github.com/shauninman/minui

---

## What's Included

This package contains:
- MinUI launcher and emulator frontend
- Base emulator cores (GB, GBC, GBA, NES, SNES, Genesis, PlayStation)
- Installation files for supported devices
- Sample ROM and BIOS folders

---

## Installation

### Before You Start

**SD Card Setup:**
- Use a reputable brand SD card
- Format as FAT32 (MBR partition table)
- For dual-SD devices: Install MinUI on the **second card** (TF2)

**Important:** Some devices require one-time NAND or TF1 modifications. Follow your device's specific instructions below.

### Anbernic RG35XX Family

**Devices:** RG35XX Plus, RG35XXH, RG35XX 2024, RG28XX, RG35XXSP, RG40XXH, RG40XXV, RG CubeXX, RG34XX, RG34XXSP

**Setup:**
1. Use stock Anbernic firmware on TF1 (quality is fine, no userdata stored there)
2. Copy `rg35xxplus/dmenu.bin` to root of TF1's "NO NAME" partition (has "anbernic" folder)
3. Copy `MinUI.zip` to root of TF2 card
4. Insert both cards and boot

**Note:** Different stock TF1 versions for different models (Plus/H/2024/SP vs 28XX/40XXH). Don't mix them.

### Anbernic RG35XX (Original)

**Setup:**
1. Use stock Anbernic firmware on TF1
2. Copy `rg35xx/dmenu.bin` to root of MISC partition on TF1
3. Copy `MinUI.zip` to root of TF2 card
4. Insert both cards and boot

### Miyoo Mini / Miyoo Mini Plus

**Standard Mini:**
1. Copy `miyoo` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device

**Mini Plus:**
1. Copy `miyoo354` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device
4. Optional: Enable RTC by creating empty file `/.userdata/miyoomini/enable-rtc`

### Miyoo A30

**Setup:**
1. Copy `miyoo` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device

### Miyoo Mini Flip

**Setup:**
1. Copy `miyoo285` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device

### Miyoo Flip

**Setup:**
1. Copy `miyoo355` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Insert SD card in **right slot** (beneath power button)
4. Boot device

### Trimui Smart / Smart Pro / Brick

**Setup:**
1. Copy `trimui` folder to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device

### Powkiddy RGB30

**Setup:**
1. Download and flash Moss to **left slot** (TF-OS): https://github.com/shauninman/Moss/releases
2. Copy `MinUI.zip` to root of **right slot** (TFGAME) SD card
3. Boot device

### MagicX Mini Zero 28

**Setup:**
1. Download and flash Moss to **left slot** (TF1/INT): https://github.com/shauninman/Moss-zero28/releases
2. Copy `magicx` folder to root of **right slot** (TF2/EXT) SD card
3. Copy `MinUI.zip` to root of TF2/EXT SD card
4. Boot device

### MagicX XU Mini M (Deprecated)

**Setup:**
1. Download and flash modified stock to **left slot** (TF1/INT): https://github.com/shauninman/Moss-magicmini/releases
2. Copy `MinUI.zip` to root of **right slot** (TF2/EXT) SD card
3. Boot device

### M17 (Deprecated)

**Setup:**
1. Copy `em_ui.sh` to SD card root
2. Copy `MinUI.zip` to SD card root
3. Boot device

### GKD Pixel / GKD Mini (Deprecated)

**Important:** Not cross-compatible with other MinUI devices. Firmware lives on SD card.

**Setup:**
1. Backup entire stock SD card (or copy everything to "stock" folder on ROMS partition)
2. Copy `gkdpixel` folder to root of ROMS partition
3. Copy `MinUI.zip` to root of ROMS partition
4. Boot stock firmware
5. Navigate to APP folder, launch "file manager"
6. Navigate to `/media/roms/gkdpixel`
7. Select `install.sh` and choose "Execute"

---

## Updating MinUI

Copy `MinUI.zip` to the root of your SD card (the one with your Roms folder). Reboot. MinUI will detect the ZIP and auto-update.

---

## Using MinUI

### Controls

**For devices without dedicated MENU button:**
- **RGB30**: Use L3 or R3 for MENU
- **M17**: Use + or - for MENU

**Volume and Brightness:**

*Modern devices* (RGB30, Miyoo Mini Plus, RG35XX Plus, Trimui Smart Pro/Brick, GKD Pixel, Miyoo A30, Miyoo Flip, Mini Zero 28):
- **Brightness**: MENU + VOLUME UP/DOWN

*Classic devices* (Miyoo Mini, Trimui Smart, M17):
- **Volume**: SELECT + L1/R1
- **Brightness**: START + L1/R1

**Sleep and Wake:**

*Power button devices* (most modern handhelds):
- **Sleep**: Press POWER
- **Wake**: Press POWER

*Physical switch devices* (Trimui Smart, M17):
- **Sleep**: Press MENU twice
- **Wake**: Press MENU
- Then flip physical power switch when asleep

**Other:**
- **Mute** (Trimui Smart Pro/Brick): FN switch (also disables rumble)

### Save States and Auto-Resume

**Quicksave:** MinUI automatically saves when you power off in-game. Next time you boot, it resumes exactly where you left off.

**Manual Save States:** Press MENU in-game to access 9 save state slots (0-8).

**Resume Options:**
- Press **A** on a game to auto-resume (loads quicksave from slot 9)
- Press **X** on a game to resume from your last manual save state

**How It Works:**
- Quicksave created when powering off (manually or after 30 seconds of sleep)
- On devices without POWER button: Press MENU twice to sleep before using power switch
- Auto-resume happens on next boot

---

## ROMs and Organization

### ROM Folders

MinUI maps ROM folders to emulators using **tags in parentheses**:

```
Roms/
├── Game Boy (GB)/
├── Game Boy Advance (GBA)/
├── Nintendo (FC)/
└── Sony PlayStation (PS)/
```

The tag (e.g., "GB", "GBA") must match an emulator pak. You can rename folders but keep the tag:
- "Nintendo Entertainment System (FC)" → "NES (FC)" ✓
- "Famicom (FC)" ✓
- "Nintendo" ✗ (missing tag)

### Combining Folders

Multiple folders with the same display name merge into one menu:

```
Roms/
├── Game Boy Advance (GBA)/      # Uses stock core
└── Game Boy Advance (MGBA)/     # Uses mgba core
```

Shows as single "Game Boy Advance" entry containing ROMs from both folders.

### Multi-Disc Games

**Single disc with multiple files:**
```
Harmful Park (English v1.0)/
├── Harmful Park (English v1.0).bin
└── Harmful Park (English v1.0).cue
```

MinUI launches the .cue file when you select the folder.

**Multi-disc games:**
```
Policenauts (English v1.0)/
├── Policenauts (English v1.0).m3u
├── Policenauts (Japan) (Disc 1).bin
├── Policenauts (Japan) (Disc 1).cue
├── Policenauts (Japan) (Disc 2).bin
└── Policenauts (Japan) (Disc 2).cue
```

Create an `.m3u` file listing each disc's .cue file:
```
Policenauts (Japan) (Disc 1).cue
Policenauts (Japan) (Disc 2).cue
```

**In-game disc changing:** Press MENU → Continue shows current disc. Use left/right to switch discs.

**Supported formats:** .bin/.cue, .iso, .chd, .pbp (official format, <2GB only)

All discs share the same memory card and save states.

### Custom Display Names

Override ROM display names with a `map.txt` file in the ROM folder:

```
neogeo.zip	.Neo Geo Bios
mslug.zip	Metal Slug
sf2.zip	Street Fighter II
```

Format: `filename.ext` + TAB + `Display Name`

Start name with `.` to hide the file (useful for BIOS files).

---

## BIOS Files

Some emulators require official BIOS files. Place them in `Bios/<TAG>/`:

```
Bios/
├── FC/disksys.rom
├── GB/gb_bios.bin
├── GBA/gba_bios.bin
├── GBC/gbc_bios.bin
├── MD/bios_CD_E.bin
│      bios_CD_J.bin
│      bios_CD_U.bin
└── PS/psxonpsp660.bin
```

**Note:** Filenames are case-sensitive.

---

## Collections

Create custom playlists with text files in the `Collections/` folder:

**Example:** `Collections/Metroid series.txt`
```
/Roms/GBA/Metroid Zero Mission.gba
/Roms/GB/Metroid II.gb
/Roms/SNES (SFC)/Super Metroid.sfc
/Roms/GBA/Metroid Fusion.gba
```

Collections appear as a separate category in the launcher.

---

## Simple Mode

Hide the Tools folder and simplify the in-game menu (replaces Options with Reset).

**Enable:** Create empty file `/.userdata/shared/enable-simple-mode`

Perfect for kids or non-technical users.

---

## Advanced

### Auto-Run Script

Run custom shell scripts on boot:

**Create:** `/.userdata/<platform>/auto.sh`

**Important:** Use Unix line endings (`\n`), not Windows (`\r\n`).

---

## Credits

**eggs** - NEON scalers, example code, endless patience
- RG35XX: https://www.dropbox.com/sh/3av70t99ffdgzk1/AAAKPQ4y0kBtTsO3e_Xlrhqha
- Miyoo Mini: https://www.dropbox.com/sh/hqcsr1h1d7f8nr3/AABtSOygIX_e4mio3rkLetWTa
- Trimui Model S: https://www.dropbox.com/sh/5e9xwvp672vt8cr/AAAkfmYQeqdAalPiTramOz9Ma

**neonloop** - Trimui toolchain, picoarch inspiration
- Repos: https://git.crowdedwood.com

**muOS community** - H700 discoveries and collaboration
- muOS: https://muos.dev
- Knulli: https://knulli.org

**JELOS/fewt** - RGB30 support, Linux kernel guidance
- JELOS: https://github.com/JustEnoughLinuxOS/distribution

**Steward** - Exhaustive device documentation
- Website: https://steward-fu.github.io/website/

**Gamma** - MagicX performance unlocking, GammaOS
- Repos: https://github.com/TheGammaSqueeze/

**BlackSeraph** - chroot expertise, GarlicOS
- Repos: https://github.com/GarlicOS

**Jim Gray** - Testing, development soundtrack
- Music: https://ourghosts.bandcamp.com/music
- Patreon: https://www.patreon.com/ourghosts/
