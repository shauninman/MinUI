# MinUI
   
My Fork of MinUI with new Audio and Vsync code to fix problems within the original MinUI causing stuttering in video as well input and audio delays.   
   
In short the original current MinUI relies on just filling up the audio buffer and then just let the audio buffer control the speed of the game by just adding new frames from the emulator core each time the audio buffer has space. This sort of works as the audio buffer is filled with samples from the emulator core and they do play back at a certain speed and so space in the buffer becomes available at this same speed.    
But while this works, it will just play at the core's original FPS because the core is outputting audio meant to play at the speed of its original FPS which for most emulators is 59.23 something as that used to be the FPS of NTSC tv's, but this doesn't actually sync up with more modern screens like in emulation handhels which run around 60fps and so this mismatched of what the game runs at and your screen refreshes at will result in small stutters and what not while playing games. To say it in one line MinUI's Vsync does absolutely nothing!    
   
Besides that basically running on a full audio buffer also means audio latency is always in max mode. Audio device is playing samples at the start of the buffer while Minarch is piling new samples up in the back of the buffer. Basically its a theme park ride with a maxed out queue. You can clearly hear the delay when pressing a button and actually hearing Link swinging his sword. In a perfect scenario you want your audio buffer empty, (no queue) so any sound added to the buffer is immediately played by your system, but yeah in a practical world a complete empty buffer is pretty much impossible as you run in situations where the audio hardware is asking for new samples but there are none as for some reason not enough where added. So you need a little buffer for some headroom and variations between adding and playing audio samples, but the goal for the most direct responsive audio is a buffer as empty as possible, while MinUI currently relies on the buffer being completely stuffed. It basically mis uses the audio buffer as a frame timer, but obviously this is not the way it should be done. 

But one even worse side effect of all this is the fact that when the buffer is full and its waiting for it to clear again to get the next frame from the emulator core it basically just locks up the whole emulation app during this time. So it will absolutely do nothing at this point not even register controller inputs. Albeit the time this lock up happends is in miliseconds, but it happends a couple of miliseconds each frame and each frame there's a change you could press a button in this exact moment and it not registering.
   
This is my attempt to fix it by rewriting the whole audio code and introducing adaptive audio sampling as well as rewriting Vsync logic in minarch code. Basically I make the audio play along with the system and I am free to speed everything up to exactly match your screens framerate instead of that I let everything run around the speed of the audio how MinUI currently works. By doing this I take back full control at the rate emulation is playing and I can align it exactly with any screens refresh rate and make everything Butter Smooth and super responsive. On top of that I can optimize for a small audio buffer with the least delay between incoming and outgoing samples to get audio as direct as possible. You want control over what plays at what speed. For example in the current MinUI there is a fast foward function but the only way to achieve that was to completely disable audio while fast forwarding because once audio is back on it can't go any faster then the game's original audio speed. 
   
It's just a hobby project I started from me loving MinUI for what it is but not being able to actually enjoying it cause of the problems it has with actually emulating games. 
   
If you like to try out my fix, the current build is for TrimUI Smart Pro and Brick. I am planning on building for other devices later too!
   
But honestely since my PR on the original MinUI was closed without really any discussion and a 3 line commment with a reason which is not even valid as libraries can be included within the binary I kinda lost my love for MinUI a little and am wondering if I should even continue working on it. I don't say my PR should have been accepted but there was no room for discussion, no contacting me whatever is needed to accept my changes in confidence, nothing, It was just an invalid argument and immediatly closed right after. I worked for many many hours on this fix as this is actually very complex to solve evidenced by MinUI doing this wrong all this time and not touching this code ever release after release and it kinda sucks to see my PR closed like this with a comment with obviously shows they didn't even really look into my hard work. While I feel like my hard work is actually something that would solve a core problem in MinUI and actually fix their emulation. But it seems their not really interested or not even aware of how wrong the current implementation is and what problems it creates.
   
So yeah maybe in the future I will replace this repo with something completely from my own, but for now you can try my rewritten MinUI version yourself in case your interested. But booting up MinUI on my Brick doesn't get me as excited anymore. 
   
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

You can [grab the latest version here](https://github.com/shauninman/MinUI/releases).

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
   
## Legacy versions

The original Trimui Model S version of MinUI (2021/04/03-2021/08/06) has been archived [here](https://github.com/shauninman/MinUI-Legacy-Trimui-Model-S).

The sequel, MiniUI for the Miyoo Mini (2022/04/20-2022/10/23), has been archived [here](https://github.com/shauninman/MiniUI-Legacy-Miyoo-Mini).

The return of MinUI for the original Anbernic RG35XX (2023/02/26-2023/03/26) has been archived [here](https://github.com/shauninman/MinUI-Legacy-RG35XX).

The current MinUI which introduced support for multiple devices starting with the Trimui Smart, Miyoo Mini (and Plus), and the original Anbernic RG35XX was released on [2023/09/22][init-release] with the initial functional commit 6 months earlier on [2023/03/27][init-commit].

[init-release]:https://github.com/shauninman/MinUI/releases/tag/v20230922b-2
[init-commit]:https://github.com/shauninman/MinUI/commit/53e0296ea5a2794290fb5783765af6cee0063445#diff-b993e61ab6e66a19b67c88cfb98261aa9267d250de8bb56463662f67aae1a558
