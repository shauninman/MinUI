# MinUI Next   
MinUI with a rebuild emulation engine, includes all original MinUI Features plus:
- Game switcher menu (Onion OS style) contributed by [@frysee](https://github.com/frysee)
- Fixed both screen tearing and sync problems of the original MinUI
- Super smooth gameplay actually in sync with your screen's refresh rate! (no more stutters!)
- Much higher audio quality, due to much more advanced resampling engine (with selectable quality setting)
- Much lower latency (average 20ms, 1 frame of 60fps)
- Color and font options to customize the MinUI screens
- Ambient light mode (Leds color based on whats on your screen)
- Lot of fixes on original like inputs sometimes not registering etc.
- Screen color temperature controls, besides the standard brightness control.
- Fully working Deep Sleep mode instead of complete power off after 2 minutes (instant on)
- Battery Monitoring with graph and everything
- Default Snes9x core instead of deprecated snes9x2005, also optimized version of PCSX Rearmed and optimized FBNeo included in extras
- FBNeo screen rotation works
- JP and partial CN/KR characters supported
- Ongoing development without downside of maintaining compatibility with legacy devices

Current supported devices:   
Trimui Brick | MinUI-20241028-0   
Trimui Smart Pro | MinUI-20231111b-2 

# Future plans roadmap
- Pal rom mode
- Bluetooth and wifi integrated
- Optional game art/media
- Configurable FN and switch buttons
- More compatibility testing with different emulators and fix/improve if nessecary
- Menu animations and color settings (maybe some kind of template system?)
- Configurable sleep mode or default auto shutdown
- Retroachievements
- Shaders
- Once everything is to my liking I will look into porting this to more devices
- Clean up all MinUI code and strip it from all stuff for legacy devices etc. 

# Super awesome contributions thank you's!
- [@frysee](https://github.com/frysee) for the super awesome game switcher PR. Truly love this feature, thank you so much for contributing it to this project!
  
# Installing   

Just copy MinUI.zip (don't unzip this just copy as zipfile) and trimui to the root of your SD Card, bootup your Trim UI device it will say installing/updating and after that just enjoy all your beloved games!

# How to use
I think most speaks for itself but here are some handy short instructions just in case:
While in menu:
- Hold start and use volume up and down to adjust brightes
- Hold select and use volume up and down to adjust color temperature
- Short press select to open game switcher menu
- Idk the rest speaks for itself I guess?
While in game:
- Menu opens in game options menu, adjusting controls, scaling and what not
- Hold menu and select at same time to open up game switcher

# Original MinUI's readme:
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

> Devices with a physical power switch
> use MENU to sleep and wake instead of
> POWER. Once asleep the device can safely
> be powered off manually with the switch.

## Supported consoles

Base:

- Game Boy
- Game Boy Color
- Game Boy Advance
- Nintendo Entertainment System
- Super Nintendo Entertainment System
- Sega Genesis
- PlayStation

Extras:

- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear
- Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy
- Arcade (cps, mame etc)

## Supported Devices

| Device | Added | Status |
| -- | -- | -- |
| Anbernic RG40xxH | MinUI-20240717-1 | WIP |
| Anbernic RG40xxV | MinUI-20240831-0 | WIP | 
| Anbernic RG CubeXX | MinUI-202401028-0 | WIP | 
| Miyoo Flip | MinUI-20250111-0 | WIP |
| Miyoo Mini (plus) | MinUI-20250111-0 | Questionable |
| Trimui Brick | MinUI-20241028-0 | Working |
| Trimui Smart Pro | MinUI-20231111b-2 | Working |

> [!NOTE]
> **Working** Works! will still profit from future updates  
> **WIP** Working on getting it ready for this device   
> **Questionable** I want to get it working on this device, but might not be possible due to HW limitations
   
