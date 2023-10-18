MinUI is a minimal launcher for the Trimui Smart, Miyoo Mini (and Plus), and RG35XX--all from the same SD card. Why? Why not?

Source:
https://github.com/shauninman/minui

----------------------------------------
Installing

PREFACE

MinUI has two essential parts: an installer/updater zip archive named "MinUI.zip" and a bootstrap file or folder with names that vary by platform.

On devices that support two SD cards (eg. RG35XX) I will use the name "TF1" to refer to the card that goes into slot one of the device. All other instances of "SD card" or "primary card" refer to the card that goes into the second slot or to the sole SD card of devices that only support a single card. To get the most out of MinUI on devices that support two SD cards you should install MinUI on the second card.

The primary card should be a reputable brand and freshly formatted as FAT32.

CAVEATS

While MinUI can be updated from any device once installed, some devices require (minor) changes to NAND or TF1 (via the aforementioned bootstrap file or folder) and therefore need to be installed from the specific device before using. The same is true when trying to use an existing card in a new device of the same type. When in doubt, follow the installation instructions; if all the necessary bits are already installed, the installer will act as an updater instead.

ALL

Preload the "Bios" and "Roms" folders then copy both to the root of your primary card.

RG35XX

MinUI is meant to be installed over a fresh copy of the stock Anbernic firmware. You can use the stock TF1 card, reports of its poor quality are greatly exaggerated and, as long as you are using the recommended two card setup, no userdata is stored on it.

Copy "dmenu.bin" to the root of the MISC partition of the TF1 card. Copy "MinUI.zip" (without unzipping) to the root of the TF2 card.

RGB30

MinUI is meant to be used with Moss installed on the SD card that goes into the left slot (labeled TF-OS) of the RGB30. Download and flash the latest version:

	https://github.com/shauninman/Moss/releases/

Copy "MinUI.zip" (without unzipping) to the root of the SD card that goes into the right slot (labeled TFGAME) of the RGB30.

MIYOO MINI PLUS

Copy the "miyoo354" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

MIYOO MINI

Copy the "miyoo" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

TRIMUI SMART

Copy the "trimui" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

----------------------------------------
Updating

RG35XX / RGB30 / MIYOO MINI / MIYOO MINI PLUS / TRIMUI SMART

Copy "MinUI.zip" (without unzipping) to the root of the SD card containing your Roms.

----------------------------------------
Shortcuts

RGB30

	Menu: L3 or R3


RG35XX / MIYOO MINI PLUS / RGB30
  
  Brightness: MENU + VOLUME UP
                  or VOLUME DOWN
  
MIYOO MINI / TRIMUI SMART

  Volume: SELECT + L or R
  Brightness: START + L or R1

RG35XX / MIYOO MINI (PLUS) / RGB30
  
  Sleep: POWER
  Wake: POWER
  
TRIMUI SMART
  
  Sleep: MENU (twice)
  Wake: MENU

----------------------------------------
Roms

Included in this zip is a "Roms" folder containing folders for each console MinUI currently supports. You can rename these folders but you must keep the uppercase tag name in parentheses in order to retain the mapping to the correct emulator (eg. "Nintendo Entertainment System (FC)" could be renamed to "Nintendo (FC)", "NES (FC)", or "Famicom (FC)"). 

When one or more folder share the same display name (eg. "Game Boy Advance (GBA)" and "Game Boy Advance (MGBA)") they will be combined into a single menu item containing the roms from both folders (continuing the previous example, "Game Boy Advance"). This allows opening specific roms with an alternate pak.

----------------------------------------
Bios

Some emulators require or perform much better with official bios. MinUI is strictly BYOB. Place the bios for each system in a folder that matches the tag in the corresponding "Roms" folder name (eg. bios for "Sony PlayStation (PS)" roms goes in "/Bios/PS/"),

Bios file names are case-sensitive:

   FC: disksys.rom
   GB: gb_bios.bin
  GBA: gba_bios.bin
  GBC: gbc_bios.bin
   PS: psxonpsp660.bin
	
----------------------------------------
Disc-based games

To streamline launching multi-file disc-based games with MinUI place your bin/cue (and/or iso/wav files) in a folder with the same name as the cue file. MinUI will automatically launch the cue file instead of navigating into the folder when selected, eg. 

  Harmful Park (English v1.0)/
    Harmful Park (English v1.0).bin
    Harmful Park (English v1.0).cue

For multi-disc games, put all the files for all the discs in a single folder. Then create an m3u file in that folder (just a text file containing the relative path to each disc's cue file on a separate line) with the same name as the folder. Instead of showing the entire messy contents of the folder, MinUI will launch the appropriate cue file, eg. For a "Policenauts" folder structured like this:

  Policenauts (English v1.0)/
    Policenauts (English v1.0).m3u
    Policenauts (Japan) (Disc 1).bin
    Policenauts (Japan) (Disc 1).cue
    Policenauts (Japan) (Disc 2).bin
    Policenauts (Japan) (Disc 2).cue

The m3u file would contain just:

  Policenauts (Japan) (Disc 1).cue
  Policenauts (Japan) (Disc 2).cue

MinUI also supports chd files and official pbp files (multi-disc pbp files larger than 2GB are not supported). Regardless of the multi-disc file format used, every disc of the same game share the same memory card and save state slots.

----------------------------------------
Collections

A collection is just a text file containing an ordered list of full paths to rom, cue, or m3u files. These text files live in the "Collections" folder at the root of your SD card, eg. "/Collections/Metroid series.txt" might look like this:

  /Roms/GBA/Metroid Zero Mission.gba
  /Roms/GB/Metroid II.gb
  /Roms/SNES (SFC)/Super Metroid.sfc
  /Roms/GBA/Metroid Fusion.gba

----------------------------------------
Advanced

MinUI can automatically run a user-authored shell script on boot. Just place a file named "auto.sh" in "/.userdata/<DEVICE>/".

----------------------------------------
Thanks

To eggs, for his NEON scalers, years of top-notch example code, and patience in the face of my endless questions.

Check out eggs' releases (includes source code): 

  RG35XX https://www.dropbox.com/sh/3av70t99ffdgzk1/AAAKPQ4y0kBtTsO3e_Xlrhqha
  Miyoo Mini https://www.dropbox.com/sh/hqcsr1h1d7f8nr3/AABtSOygIX_e4mio3rkLetWTa
  Trimui Model S https://www.dropbox.com/sh/5e9xwvp672vt8cr/AAAkfmYQeqdAalPiTramOz9Ma

To neonloop, for putting together the original Trimui toolchain from which I learned everything I know about docker and buildroot and is the basis for every toolchain I've put together since, and for picoarch, the inspiration for minarch.

Check out neonloop's repos: 

  https://git.crowdedwood.com

To BlackSeraph, for introducing me to chroot.

And to Jim Gray, for commiserating during development, for early alpha testing, and for providing the soundtrack for much of MinUI's development.

Check out Jim's music: 

  https://ourghosts.bandcamp.com/music
  https://www.patreon.com/ourghosts/

----------------------------------------
Known Issues

RGB30

- garbage may be drawn below aspect scaled systems
- some systems have (usually subtle) audio clipping
- some systems need additional performance tuning

TRIMUI SMART

- debug/battery overlay isn't implemented yet

MIYOO MINI / MIYOO MINI PLUS

- battery overlay isn't implemented yet
