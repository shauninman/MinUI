# MinUI Next   
MinUI with a rebuild emulation engine
   
# Installing   

Just copy MinUI.zip and trimui to your SD Card, bootup your Trim UI device and play magic!   
   
Current supported devices:   
Trimui Brick | MinUI-20241028-0   
Trimui Smart Pro | MinUI-20231111b-2   

# Why does this exist what is the problem with MinUI?   
      
In short the original current MinUI relies on just filling up the audio buffer and then just let the audio buffer control the speed of the game by just adding new frames from the emulator core each time the audio buffer has space. This sort of works as the audio buffer is filled with samples from the emulator core and they do play back at a certain speed and so space in the buffer becomes available at this same speed.    
But while this works, it will just play at the core's original FPS because the core is outputting audio meant to play at the speed of its original FPS which for most emulators is 59.23 something as that used to be the FPS of NTSC tv's, but this doesn't actually sync up with more modern screens like in emulation handhels which run around 60fps and so this mismatched of what the game runs at and your screen refreshes at will result in small stutters and what not while playing games. To say it in one line MinUI's Vsync does absolutely nothing!    
   
Simple version of the OG MinUI code works like, audio buffer full? repeat the last audio sample process and try 10 times with an SDL_Delay(1) between each try and hope within 10 tries there is enough room again in the audio buffer to display another frame from the game lol. Then there are situations where there is no audio, so no full audio buffer to slow it down, like when in the menu's etc so to stop MinUI from running at infinite FPS in those situations, theres like a Sync() function which basically does SDL_Delay(60fps)/17ms and basically all over the place there is code to use this Sync() function for when there is no audio buffer slowing the whole bunch down. I am not joking, this is really how the current MinUI code works...
   
So besides the obvious game is not running in sync with your screen at all is that running on a full audio buffer also means audio latency is always in max mode. Audio device is playing samples at the start of the buffer while Minarch is piling new samples up in the back of the buffer. Basically its a theme park ride with a maxed out queue. You can clearly hear the delay when pressing a button and actually hearing Link swinging his sword. In a perfect scenario you want your audio buffer empty, (no queue) so any sound added to the buffer is immediately played by your system, but yeah in a practical world a complete empty buffer is pretty much impossible as you run in situations where the audio hardware is asking for new samples but there are none as for some reason not enough where added. So you need a little buffer for some headroom and variations between adding and playing audio samples, but the goal for the most direct responsive audio is a buffer as empty as possible, the opposite of how MinUI currently relies on the buffer being completely stuffed to run at normal speed. It basically abuses the audio buffer as a frame speed timer, but obviously this is not the way it should be done. 

But one even worse side effect of all this is the fact that when the buffer is full and its waiting for it to clear again to get the next frame from the emulator core it basically just locks up the whole emulation app during this time. So it will absolutely do nothing at this point not even register controller inputs. Albeit the time this lock up happends is in miliseconds, but it happends a couple of miliseconds each frame and each frame there's a change you could press a button in this exact moment and it not registering.
   
This is my attempt to fix it by rewriting the whole audio code and introducing adaptive audio sampling as well as rewriting Vsync logic in minarch code. Basically I make the audio play along with the system and I am free to speed everything up to exactly match your screens framerate instead of that I let everything run around the speed of the audio how MinUI currently works. By doing this I take back full control at the rate emulation is playing and I can align it exactly with any screens refresh rate and make everything Butter Smooth and super responsive. On top of that I can optimize for a small audio buffer with the least delay between incoming and outgoing samples to get audio as direct as possible. You want control over what plays at what speed. For example in the current MinUI there is a fast foward function but the only way to achieve that was to completely disable audio while fast forwarding because once audio is back on it can't go any faster then the game's original audio speed. 
   
It's just a hobby project I started from me loving MinUI for what it is but not being able to actually enjoying it cause of the problems it has with actually emulating games. 
   
# So why is this fix not in the original MinUI?   
Well I did do a PR on the original MinUI, but it got declined. Personally I feel for invalid reasons as it could have been solved by rebuilding part of my code. But the OG creator just denied it immediately without leaving room for any discussion and changes. For me it felt a bit like disrespect towards my hard work on trying to help fixing a core problem in MinUI. Their reason for closing was that because I use libsamplerate and use more accurate SDL2 timings it won't run or atleast needs to be tested on older devices, specially arm32 based devices as those libraries could maybe not run on those devices. At first I said this is total bullshit because I statically link the library, but during my process of creating this I find out this is actually somewhat true (altough I could spend a bit more time on this to be sure), so yeah have to give it to them on this part, but still doesn't take away the fact my logic could still be changed to work without samplerate and SDL1 (altough it won't be as good, altough would still be a lot better than what MinUI is doing now).
    
So yeah this expirience made me feel to not contribute anymore to the orignal MinUI and continue this as a standalone version instead   
   
But yeah now this is my own version I stand for a choice to rebuild parts of my code to lesser optimal versions without libsamplerate and SDL1 to support all devices the original MinUI does or do I choose to just support more recent devices so I can continue using more optimal code and better results. Right now I am going for the second choice as initially I was building this for myself and its still my main reasoning for doing this. Also I want to build towards the future and sometimes that means letting go of compatibility from older hardware.

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
   
