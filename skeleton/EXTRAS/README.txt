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

----------------------------------------
Native PICO-8 and Splore.pak (RGB30-only)

Download the official PICO-8 fantasy console for the Raspberry Pi from https://lexaloffle.itch.io/pico-8 (If you've bought a bundle on itch.io any time in the last few years you might already have a copy in your library https://itch.io/my-collections) At the time of writing, the file is named "pico-8_0.2.5g_raspi.zip". Copy that zip file into "/Tools/rgb30/Splore.pak/". Copy "/Emus/rgb30/P8-NATIVE.pak", "/Tools/rgb30/Splore.pak", and "/Tools/rgb30/Wi-Fi.pak" to your SD card. (You need the Wi-Fi.pak to download PICO-8 games in Splore.pak.)

Place carts you would like to play with native PICO-8 in "/Roms/Pico-8 (P8-NATIVE)/".

To exit P8-NATIVE.pak, press start. Then select "SHUTDOWN".

To exit Splore.pak, press start. In-game select "EXIT TO SPLORE". With a cart selected in splore press start, select "OPTIONS", and then "SHUTDOWN PICO-8". 

Please note that Splore.pak and P8-NATIVE.pak don't/can't implement MinUI's uniform features like the in-game menu (including save states), faux sleep, and quicksave/auto-resume.

----------------------------------------
Wi-Fi.pak (RGB30-only)

Open "wifi.txt" in a plain text editor and enter your network name and password on two separate lines and save, eg.

  minui
  lessismore

Copy "Wi-Fi.pak" to "/Tools/rgb30/" on your SD card. The first time you open the pak (or any time you change the contents of "wifi.txt") it will update the network name and password then connect. Subsequent launches will toggle wi-fi on and off. Wi-fi will drain the battery so it's best to only enable it when you plan to use it and disable it when you're done. Your Wi-fi state persists across reboots.

----------------------------------------
Remove Loading.pak (Miyoo A30-only)

This pak removes the "LOADING" text that appears between the boot logo and the main interface when powering on. It requires patching nand memory to modify the otherwise read-only root file system. It takes around 5 minutes to complete. While not required it is a very good idea to be connected to power while performing this operation. Please be patient and do not power off the device before it completes.

----------------------------------------
Remove Loading.pak (Trimui Smart Pro-only)

This pak removes the "LOADING" text that appears between the boot logo and the main interface when powering on. Compared to the A-30 this is a minimally invasive procedure. Nothing to worry about.
