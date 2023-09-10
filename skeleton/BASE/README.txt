MinUI is a minimal launcher for the Miyoo Mini (and Plus), Trimui Smart, and RG35XX

Source:
https://github.com/shauninman/minui-minarch-et-al

----------------------------------------
Installing

PREFACE

MinUI has two essential parts: an installer/updater zip archive named "MinUI.zip" and a bootstrap file or folder with names that vary by platform.

On devices that support two SD cards (eg. RG35XX) I will use the name "TF1" to refer to the card that goes into slot one of the device. All other instances of "SD card" or "primary card" refer to the card that goes into the second slot or to the sole SD card of devices that only support a single card. To get the most out of MinUI on devices that support two SD cards you should install MinUI on the second card.

The primary card should be a reputable brand and freshly formatted as FAT32.

CAVEATS

While MinUI can be updated from any device once installed, some devices require (minor) changes to NAND or TF1 and therefore need to be installed from the specific device before using. For example, installing MinUI on the RG35XX or Trimui Smart allows it to work on the Miyoo Mini family. But installing on a Miyoo Mini device would require installing again directly on the RG35XX and/or Trimui Smart. When in doubt follow the installation instructions; if all the necessary bits are already installed, the installer will act as an updater instead.

RG35XX

MinUI is meant to be installed over a fresh copy of the stock Anbernic firmware. You can use the stock TF1 card, reports of its poor quality are greatly exaggerated and, as long as you are using the recommended two card setup, no userdata is stored on it.

Copy "dmenu.bin" to the root of the MISC partition of the TF1 card. Copy "MinUI.zip" (without unzipping) to the root of the TF2 card.

MIYOO MINI PLUS

Copy the "miyoo354" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

MIYOO MINI

Copy the "miyoo" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

TRIMUI SMART

Copy the "trimui" folder and "MinUI.zip" (without unzipping) to the root of the SD card.

----------------------------------------
Updating

RG35XX

Copy "MinUI.zip" (without unzipping) to the root of your primary card.

MIYOO MINI / MIYOO MINI PLUS / TRIMUI SMART

Copy "MinUI.zip" (without unzipping) to the root of the SD card.

----------------------------------------
Shortcuts

RG35XX / MIYOO MINI PLUS
  
  Brightness: MENU + VOLUME UP
                  or VOLUME DOWN
  
MIYOO MINI / TRIMUI SMART

  Volume: SELECT + L or R
  Brightness: START + L or R1

RG35XX / MIYOO MINI / MIYOO MINI PLUS
  
  Sleep: POWER
  Wake: POWER
  
TRIMUI SMART
  
  Sleep: MENU (twice)
  Wake: MENU

----------------------------------------
Known Issues

TRIMUI SMART

- debug/battery overlay isn't implemented yet

MIYOO MINI / MIYOO MINI PLUS

- battery overlay isn't implemented yet
