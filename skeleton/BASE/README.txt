MinUI is a minimal launcher for the Trimui Smart (and Pro), the Miyoo Mini (and Plus) and A30, the Powkiddy RGB30, the M17, the MagicX XU Mini M, and the Anbernic RG*XX family--all from the same SD card. Why? Why not?

Source:
https://github.com/shauninman/minui

----------------------------------------
Installing

PREFACE

MinUI has two essential parts: an installer/updater zip archive named "MinUI.zip" and a bootstrap file or folder with names that vary by platform.

On devices that support two SD cards (eg. RG35XX) I will use the name "TF1" to refer to the card that goes into slot one of the device. All other instances of "SD card" or "primary card" refer to the card that goes into the second slot or to the sole SD card of devices that only support a single card. To be able to use MinUI from a single SD card on multiple devices you must install it on the second card of devices that support two SD cards.

The primary card should be a reputable brand and freshly formatted as FAT32.

CAVEATS

While MinUI can be updated from any device once installed, some devices require (minor) changes to NAND or TF1 (via the aforementioned bootstrap file or folder) and therefore need to be installed from the specific device before using. The same is true when trying to use an existing card in a new device of the same type. When in doubt, follow the installation instructions; if all the necessary bits are already installed, the installer will just act as an updater instead.

ALL

Preload the "Bios" and "Roms" folders then copy both to the root of your primary card.

RGB30

MinUI is meant to be used with Moss installed on the SD card that goes into the left slot (labeled TF-OS) of the RGB30. Download and flash the latest version:

	https://github.com/shauninman/Moss/releases/

Copy "MinUI.zip" (without unzipping) to the root of the SD card that goes into the right slot (labeled TFGAME) of the RGB30.

MAGICX XU MINI M

MinUI is meant to be used with a heavily modified stock SD card that goes into the left slot (labeled TF1/INT). Download and flash the latest version:

	https://github.com/shauninman/Moss-magicmini/releases

Copy "MinUI.zip" (without unzipping) to the root of the SD card that goes into the right slot (labeled TF2/EXT).

TRIMUI SMART / TRIMUI SMART PRO

Copy the "trimui" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

MIYOO MINI / MIYOO A30

Copy the "miyoo" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

MIYOO MINI PLUS

Copy the "miyoo354" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

M17

Copy the "em_ui.sh" file and "MinUI.zip" (without unzipping) to the root of the SD card.

RG35XX

MinUI is meant to be installed over a fresh copy of the stock Anbernic firmware. You can use the stock TF1 card, reports of its poor quality are greatly exaggerated and, as long as you are using the recommended two card setup, no userdata is stored on it.

Copy "/rg35xx/dmenu.bin" (just the file) to the root of the MISC partition of the TF1 card. Copy "MinUI.zip" (without unzipping) to the root of the TF2 card.

RG35XX PLUS / RG35XX H / RG35XX 2024 / RG28XX / RG35XXSP / RG40XXH / RGCUBEXX

MinUI is meant to be installed over a fresh copy of the stock Anbernic firmware. You can use the stock TF1 card, reports of its poor quality are greatly exaggerated and, as long as you are using the recommended two card setup, no userdata is stored on it. (Note that the PLUS/H/2024/SP stock TF1 is not compatible with the 28XX/40XXH and vice versa.)

Copy "/rg35xxplus/dmenu.bin" (just the file) to the root of the "NO NAME" partition (FAT32 with an "anbernic" folder) of the TF1 card. Copy "MinUI.zip" (without unzipping) to the root of the TF2 card.

GKD PIXEL / GKD MINI

An important caveat: this device is not cross-compatible with other MinUI-supported devices because its firmware lives on the SD card and its presence trips up all the other devices.

Backup your stock SD card (not just the "ROMS" partition but the entire thing). If you like to live on the edge just create a folder named "stock" on the "ROMS" partition and copy everything into that folder.

Copy the "gkdpixel" folder and "MinUI.zip" (without unzipping) to the root of the "ROMS" partition of the SD card. (On the GKD Mini should be TF1.)

Boot stock, navigate to the "APP" folder and launch "file manager". Then use the d-pad and A button to navigate to "/media/roms/gkdpixel". Highlight the "install.sh" file and press A to open a menu and select "Execute" to install MinUI.

----------------------------------------
Updating

ALL

Copy "MinUI.zip" (without unzipping) to the root of the SD card containing your Roms.

----------------------------------------
Shortcuts

For devices without a dedicated MENU button

	RGB30: use L3 or R3 for MENU
	M17:   use + or - for MENU

RGB30 / MIYOO MINI PLUS / RG35XX (PLUS) / TRIMUI SMART PRO / GKD PIXEL / MIYOO A30 / MAGICX XU MINI M
  
  Brightness: MENU + VOLUME UP
                  or VOLUME DOWN
  
MIYOO MINI / TRIMUI SMART / M17

  Volume: SELECT + L or R
  Brightness: START + L or R

RGB30 / MIYOO MINI (PLUS) / RG35XX (PLUS) / TRIMUI SMART PRO / GKD PIXEL / MIYOO A30
  
  Sleep: POWER
  Wake: POWER
  
TRIMUI SMART / M17
  
  Sleep: MENU (twice)
  Wake: MENU

----------------------------------------
Quicksave & auto-resume

MinUI will create a quicksave when powering off in-game. The next time you power on the device it will automatically resume from where you left off. A quicksave is created when powering off manually or automatically after a short sleep. On devices without a POWER button (eg. the Trimui Smart or M17) press the MENU button twice to put the device to sleep before flipping the POWER switch.

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
   MD: bios_CD_E.bin
       bios_CD_J.bin
	   bios_CD_U.bin
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

When a multi-disc game is detected the in-game menu's Continue item will also show the current disc. Press left or right to switch between discs.

MinUI also supports chd files and official pbp files (multi-disc pbp files larger than 2GB are not supported). Regardless of the multi-disc file format used, every disc of the same game share the same memory card and save state slots.

----------------------------------------
Collections

A collection is just a text file containing an ordered list of full paths to rom, cue, or m3u files. These text files live in the "Collections" folder at the root of your SD card, eg. "/Collections/Metroid series.txt" might look like this:

  /Roms/GBA/Metroid Zero Mission.gba
  /Roms/GB/Metroid II.gb
  /Roms/SNES (SFC)/Super Metroid.sfc
  /Roms/GBA/Metroid Fusion.gba

----------------------------------------

Display names

Certain (unsupported arcade) cores require roms to use arcane file names. You can override the display name used throughout MinUI by creating a map.txt in the same folder as the files you want to rename. One line per file, `rom.ext` followed by a single tab followed by `Display Name`. You can hide a file by adding a `.` at the beginning of the display name. eg.
	
	neogeo.zip	.Neo Geo Bios
	mslug.zip	Metal Slug
	sf2.zip	Street Fighter II

----------------------------------------
Simple mode

Not simple enough for you (or maybe your kids)? MinUI has a simple mode that hides the Tools folder and replaces Options in the in-game menu with Reset. Perfect for handing off to littles (and olds too I guess). Just create an empty file named "enable-simple-mode" (no extension) in "/.userdata/shared/".

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

To adixial and acmeplus and the entire muOS community, for sharing their discoveries for the h700 family of Anbernic devices.

Check out muOS and Knulli:

	https://muos.dev
	https://knulli.org

To fewt and the entire JELOS community, for JELOS (without which MinUI would not exist on the RGB30) and for sharing their knowledge with this perpetual Linux kernel novice.

Check out JELOS:

  https://github.com/JustEnoughLinuxOS/distribution

To Steward, for maintaining exhaustive documentation on a plethora of devices:

	https://steward-fu.github.io/website/

To Gamma, for his efforts unlocking more performance on the MagicX XU Mini M (before we all realized its RG3562 was surreptitiously an RK3266).

Check out his repos (including GammaOS):

	https://github.com/TheGammaSqueeze/

To BlackSeraph, for introducing me to chroot.

Check out the GarlicOS repos:

	https://github.com/GarlicOS

To Jim Gray, for commiserating during development, for early alpha testing, and for providing the soundtrack for much of MinUI's development.

Check out Jim's music: 

  https://ourghosts.bandcamp.com/music
  https://www.patreon.com/ourghosts/
 
