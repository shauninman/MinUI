# NextUI (formerly MinUI Next)
MinUI with a rebuild emulation engine and tons of added features
- Fixed both screen tearing and sync stutter problems of original MinUI by rebuilding the emulator engine core
- Game switcher menu (Onion OS style) by [@frysee](https://github.com/frysee)
- High audio quality, due to advanced resampling engine using [libsamplerate](https://github.com/libsndfile/libsamplerate) (with quality/performance setting per emulator)
- Much lower latency (average 20ms, 1 frame of 60fps)
- Color and font settings to customize the NextUI interface
- Fully Integrated LEDS control, change colors, effects, brightness and also LED indicators for example standby mode indicator
- Ambient LED mode, the LEDS act like Ambient light TV's for extra immersion, configurable per emulator
- Screen color temperature and brightness controls
- Deep Sleep mode, gives instant ON and avoids the overheat bug on the Brick by [@zhaofengli](https://github.com/zhaofengli)
- Battery Monitoring including history graph and time left prediction
- Updated and optimized build emulation cores
- Rumble strength fixed and is now variable as used by the games
- FBNeo Arcade screen rotation
- Next font supports CJK for JP/CN etc rom names
- Lot of other smaller fixes and optimizations

Current supported devices:   
Trimui Brick | MinUI-20241028-0   
Trimui Smart Pro | MinUI-20231111b-2 

# Future plans roadmap
- Pal rom mode
- Bluetooth and wifi integrated
- Optional game art/media
- Configurable FN and switch buttons
- More compatibility testing with different emulators and fix/improve if nessecary
- Menu animations
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
    
Deep sleep:   
- First the device goes into light sleep mode the screen turns off and the Leds will pulse 5 times to let you know its in light sleep, after 2 minutes the device will go in full deep sleep and the leds will also turn completely off. 

![gameswitcher](https://github.com/user-attachments/assets/4c71dc26-d071-48cf-836e-83bd9a248a32)
![battery](https://github.com/user-attachments/assets/5f8a6f85-7bb7-41b0-95ab-468229a7f443)
![battery](https://github.com/user-attachments/assets/9e7c14b3-757d-4e01-b381-71897e6dc4e2)
![ingamesettings](https://github.com/user-attachments/assets/73fbed30-7aaa-420b-bb53-74dd50160434)
![gamelist](https://github.com/user-attachments/assets/ed0d2552-04c1-40a3-9eb2-14406e83b09a)
![gameoverlay](https://github.com/user-attachments/assets/a7c99784-fa48-4d3e-a64b-28e7149d929a)

# Original MinUI's readme:
MinUI is a focused, custom launcher and libretro frontend for [a variety of retro handhelds](#supported-devices).

 

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
   
