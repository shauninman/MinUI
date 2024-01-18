MinUI is a minimal launcher for the RGB30, Trimui Smart (and Pro), Miyoo Mini (and Plus), M17, and RG35XX--all from the same SD card. Why? Why not?

Source:
https://github.com/shauninman/minui

----------------------------------------
Installing

PREFACE

On devices that support two SD cards (eg. RG35XX) I will use the name "TF1" to refer to the card that goes into slot one of the device. All other instances of "SD card" or "primary card" refer to the card that goes into the second slot or to the sole SD card of devices that only support a single card. To get the most out of MinUI on devices that support two SD cards you should install MinUI on the second card.

Copy all the folders from this zip file to the root of your primary card.

----------------------------------------
Updating

This extras zip file is included with every release, regardless of whether its contents has changed or not. Refer to the release notes to see what, if anything, was added or changed and copy just the desired updated pak(s) to the corresponding device folder in the Emus or Tools folders (eg. /Emus/tg5040 or /Tools/rgb30)

----------------------------------------
Bios files

You'll need to BYOB for the emulator paks included in this zip file.

MGBA: gba_bios.bin
 PCE: syscard3.pce
 PKM: bios.min
 SGB: sgb.bios
 MD: bios_CD_E.bin (for SegaCD european games)
 MD: bios_CD_J.bin (for SegaCD japan games)
 MD: bios_CD_U.bin (for SegaCD USA games)

----------------------------------------
Splore.pak (RGB30-only)

Download the official PICO-8 fantasy console for the Raspberry Pi from https://lexaloffle.itch.io/pico-8 (If you've bought a bundle on itch.io any time in the last few years you might already have a copy in your library https://itch.io/my-collections) At the time of writing, the file is named "pico-8_0.2.5g_raspi.zip". Copy that zip file into "Splore.pak" before copying the pak to "/Tools/rgb30/" on your SD card. You will also need the Wi-Fi.pak to download PICO-8 games.

To exit Splore.pak, press start. In-game select "EXIT TO SPLORE". With a cart selected in splore press start, select "OPTIONS", and then "SHUTDOWN PICO-8".

Because Splore.pak doesn't/can't implement MinUI's uniform features like the in-game menu, faux sleep, and quicksave/auto-resume, it is currently (and will likely remain) classified as a Tool.

----------------------------------------
Wi-Fi.pak (RGB30-only)

Open "wifi.txt" in a plain text editor and enter your network name and password on two separate lines and save, eg.

  minui
  lessismore

Copy "Wi-Fi.pak" to "/Tools/rgb30/" on your SD card. The first time you open the pak (or any time you change the contents of "wifi.txt") it will update the network name and password then connect. Subsequent launches will toggle wi-fi on and off. Wi-fi will drain the battery so it's best to only enable it when you plan to use it and disable it when you're done. Your Wi-fi state persists across reboots.
