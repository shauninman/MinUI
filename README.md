<div align="center">

  <img src="github/logo.png">

  (formerly MinUI Next)

</div>

<hr>

<h4 align="center">
  <a href="https://github.com/LoveRetro/NextUI/wiki/Installation#installation--update">Install</a>
  ·
  <a href="https://github.com/LoveRetro/NextUI/wiki">Wiki</a>
  ·
  <a href="https://discord.gg/HKd7wqZk3h">Discord</a>
</h4>

<div align="center"><p>
    <a href="https://github.com/LoveRetro/NextUI/releases/latest">
      <img alt="Latest release" src="https://img.shields.io/github/v/release/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=C9CBFF&logoColor=D9E0EE&labelColor=302D41&include_prerelease&sort=semver" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/pulse">
      <img alt="Last commit" src="https://img.shields.io/github/last-commit/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=8bd5ca&logoColor=D9E0EE&labelColor=302D41"/>
    </a>
    <a href="https://github.com/LoveRetro/NextUI/blob/main/LICENSE">
      <img alt="License" src="https://img.shields.io/github/license/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=ee999f&logoColor=D9E0EE&labelColor=302D41" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/stargazers">
      <img alt="Stars" src="https://img.shields.io/github/stars/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=c69ff5&logoColor=D9E0EE&labelColor=302D41" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/issues">
      <img alt="Issues" src="https://img.shields.io/github/issues/LoveRetro/NextUI?style=for-the-badge&logo=bilibili&color=F5E0DC&logoColor=D9E0EE&labelColor=302D41" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI">
      <img alt="Repo Size" src="https://img.shields.io/github/repo-size/LoveRetro/NextUI?color=%23DDB6F2&label=SIZE&logo=codesandbox&style=for-the-badge&logoColor=D9E0EE&labelColor=302D41" />
    </a>
</div>

A CFW based of MinUI with a rebuild emulation engine and tons of added features for the TrimUI Brick and Smart Pro. For other devices we recommend checking out [MinUI](https://github.com/shauninman/MinUI)

- Fixed both screen tearing and sync stutter problems of MinUI by rebuilding the emulator engine core
- Game switcher menu (Onion OS style) by [@frysee](https://github.com/frysee)
- High audio quality, due to advanced resampling engine using [libsamplerate](https://github.com/libsndfile/libsamplerate) (with quality/performance setting per emulator)
- Much lower latency (average 20ms, 1 frame of 60fps)
- Game art/media support
- Game time tracker!
- Cheats support
- Dynamic cpu speed scaling (keeps your device cool and extends battery life, but gives the performance right when needed)
- Custom bootlogos contributed by [@SolvalouArt](https://bsky.app/profile/solvalouart.bsky.social)
- Color and font settings to customize the NextUI interface
- Menu animations
- LED control, change colors, effects, brightness
- LED indicators, low battery, stand by, charging (brightness can be set seperately)
- Ambient LED mode, the LEDS act like Ambient light TV's for extra immersion, configurable per emulator
- Screen color temperature and brightness controls
- Deep Sleep mode, gives instant ON and avoids the overheat bug on the Brick by [@zhaofengli](https://github.com/zhaofengli)
- Battery Monitoring including history graph and time left prediction
- Scrolling animated titles for longer game names
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
- Configurable FN and switch buttons
- More compatibility testing with different emulators and fix/improve if nessecary
- Retroachievements
- Shaders
- Probably we think of a lot of other stuff a long the way to add :D
- Once everything is to my liking I will look into porting this to more devices
- Clean up all MinUI code and strip it from all stuff for legacy devices etc.

# Super awesome contributions thank you's

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

# What about X feature?

While features still missing will be eventually be part native on NextUI, there are right now already some excellent Paks out there from other developers. For for example things like WiFI and syncthings we def recommend checking out the awesome work from [@josegonzalez](https://github.com/josegonzalez) He has an awesome Pak library with many things
<https://github.com/josegonzalez/pakman/>

## Watch this awesome review by Play Retro

[![Watch this awesome review by Play Retro](https://img.youtube.com/vi/m_7ObMHAFK4/maxresdefault.jpg)](https://www.youtube.com/watch?v=m_7ObMHAFK4)

## Media

![gameswitcher](https://github.com/user-attachments/assets/4c71dc26-d071-48cf-836e-83bd9a248a32)
![battery](https://github.com/user-attachments/assets/5f8a6f85-7bb7-41b0-95ab-468229a7f443)
![battery](https://github.com/user-attachments/assets/9e7c14b3-757d-4e01-b381-71897e6dc4e2)
![ingamesettings](https://github.com/user-attachments/assets/73fbed30-7aaa-420b-bb53-74dd50160434)
![gamelist](https://github.com/user-attachments/assets/ed0d2552-04c1-40a3-9eb2-14406e83b09a)
![gameoverlay](https://github.com/user-attachments/assets/a7c99784-fa48-4d3e-a64b-28e7149d929a)

# MinUI Readme

NextUI is based of [MinUI](https://github.com/shauninman/MinUI) which is an amazing CFW and works on many more devices than NextUI currently does, so def check it out!

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
