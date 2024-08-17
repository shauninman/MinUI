# About MinUI paks

A pak is just a folder with a ".pak" extension that contains a shell script named "launch.sh". 

There are two kinds of paks, emulators and tools. Emulator paks live in the Emus folder. Tool paks live in the Tools folder. These two folders live at the root of your SD card. Extra paks should never be added to the hidden ".system" folder at the root of the SD card. This folder is deleted and replaced every time a user updates MinUI.

Paks are platform specific. Inside the Emus and Tools folders you will find (or need to create) platform folders. Some platform folders are named after the target device (eg. "rgb30" for the Powkiddy RGB30), others use the device's internal name (eg. "tg5040" for the Trimui Smart Pro), other use an arbitrary shortname (eg. "trimui" for the Trimui Model S), all are completely lowercase. See the extras bundle for up-to-date supported platform folder names.

# The types of emulator pak

There are three basic types of emulator paks, which you chose depends on your goals and your desired level of MinUI integration.

The first type reuses a libretro core included with a base MinUI install. This takes advantage of a known working core but allows customizing the default options and separating user configs. An example of this type is the extra GG.pak which uses the default picodrive core.

The second type includes its own libretro core. This allows you to support completely new systems while still taking advantage of MinUI's standard features like resume from menu, quicksave and auto-resume, and consistent in-game menus, behaviors, and options. An example of this type is the extra MGBA.pak which bundles its own mgba core.

The third type launches a bundled standalone emulator. This may allow you to squeeze more performance out of a piece of hardware than a libretro core could. The downside of this type is no integration with MinUI. No resume from menu, no quicksave and auto-resume, no consistent in-game menus, behaviors, or options. In some cases the MENU (and if available, POWER) button may not function as expected, if at all. This type of pak should be a last resort. An example of this type is the community developed NDS.pak which is available for a handful of platforms MinUI supports.

In all cases please make clear to your users that I (@shauninman) can't support third-party paks. If I've excluded a console or core from MinUI's base or extra bundles it's usually for good reason, either the core's integration wasn't up to snuff (eg. arcade cores expect roms to have specific, arcane file names with only certain rom sets working with certain cores), has too many bugs (eg. unable to reliably resume from a save state), has poor performance on a given device, or is just a console I have no familiarity with or interest in.

# Naming your emulator pak

MinUI maps roms to paks based on the tag in parentheses at the end the name of the rom's parent folder (eg. "/Roms/Game Boy (GB)/Dr. Mario (World).gb" will launch the "GB.pak"). A tag should be all uppercase. When choosing a tag, start with common abbreviations used by other emulation frontends like Retroarch or EmulationStation (eg. FC for Famicom/Nintendo or MD for MegaDrive/Genesis). If that tag is already being used by another pak, use the core name if short (eg. MGBA) or an abbreviation (eg. PKM for pokemini) or truncation (eg. SUPA for mednafen_supafaust) of the core name.

# Launching your core

Here's an example "launch.sh":

	#!/bin/sh
	
	EMU_EXE=picodrive
	
	###############################
	
	EMU_TAG=$(basename "$(dirname "$0")" .pak)
	ROM="$1"
	mkdir -p "$BIOS_PATH/$EMU_TAG"
	mkdir -p "$SAVES_PATH/$EMU_TAG"
	HOME="$USERDATA_PATH"
	cd "$HOME"
	minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" &> "$LOGS_PATH/$EMU_TAG.txt"

This will open the requested rom using the "picodrive\_libretro.so" core included with the base MinUI install. To use a different core just change the value of `EMU_EXE` to another core name (minus the "_libretro.so"). If that core is bundled in your pak add the following after the `EMU_EXE` line:

	CORES_PATH=$(dirname "$0")

There's no need to edit anything below the line of hash marks. The rest is boilerplate that will extract the pak's tag from its folder name, create corresponding bios and save folders, set the `HOME` env to "/.userdata/[platform]/", launch the game, and log any output from minarch and the core to "/.userdata/[platform]/logs/[TAG].txt".

That's it! Feel free to experiement with cores from the stock firmware, other compatible devices, or building your own.

Oh, if you're creating a pak for Anbernic's RG*XX line you'll need to change the last part of the last line from ` &> "$LOGS_PATH/$EMU_TAG.txt"` to ` > "$LOGS_PATH/$EMU_TAG.txt" 2>&1` because its default shell is whack.

# Option defaults and button bindings

Copy your new pak and some roms to your SD card and launch a game. Press the MENU button and select Options. Configure the Frontend, Emulator, and Controls. MinUI standard practice is to only bind controls present on the physical controller of the original system (eg. no turbo buttons or core-specific features like palette or disk switching). Let the player dig into that if they want to, the same goes for Shortcuts. Finally select Save Changes > Save for Console. Then quit and pop your SD card back into your computer. 

Inside the hidden ".userdata" folder at the root of your SD card, you'll find platform folders, and inside your platform folder a "[TAG]-[core]" folder. Copy the "minarch.cfg" file found within to your pak folder and rename it "default.cfg". Open "default.cfg" and delete any options you didn't customize. Any option name prefixed with a "-" will be set and hidden. This is useful for disabling features that may not be available (eg. overclocking) or perform poorly (eg. upscaling) on a specific platform. Near the bottom of the file you will find the button bindings. Here's an example from the "MGBA.pak":

	bind Up = UP
	bind Down = DOWN
	bind Left = LEFT
	bind Right = RIGHT
	bind Select = SELECT
	bind Start = START
	bind A Button = A
	bind B Button = B
	bind A Turbo = NONE:X
	bind B Turbo = NONE:Y
	bind L Button = L1
	bind R Button = R1
	bind L Turbo = NONE:L2
	bind R Turbo = NONE:R2
	bind More Sun = NONE:L3
	bind Less Sun = NONE:R3

Everything after `bind ` up to the `=` is the button label that will appear in the Controls menu. I usually normalize these labels (eg.  "Up" instead of "D-pad up", "A Button" instead of just "A"). Everything after the `=` up to the optional `:` is the button mapping. Button mappings are all uppercase. Shoulder buttons and analog stick buttons always include the number, (eg. "L1" instead of just "L"). Use "NONE" if the button should not be bound by default. When customizing or removing a binding, the default core-defined button mapping should always be added after a ":". In the example above, I removed the default "More Sun" binding by changing:

	bind More Sun = L3

to

	bind More Sun = NONE:L3

# Brightness and Volume

Some binaries insist on resetting brightness (eg. DinguxCommander on the 40xxH stock firmware) or volume (eg. ppssppSDL everywhere) on every launch. To keep this in sync with MinUI's global settings there's syncsettings.elf. It waits one second then restores MinUI's current brightness and volume settings. In most cases you can just launch it as a daemon before launching the binary:

	syncsettings.elf &
	./DinguxCommander

But if a binary takes more than one second to initialize you might need to just let it run in a loop the entire time the binary is running:

	while :; do
	    syncsettings.elf
	done &
	LOOP_PID=$!
	
	./PPSSPPSDL --pause-menu-exit "$ROM_PATH"
	
	kill $LOOP_PID

# Caveats

MinUI currently only supports the RGB565 pixel format and does not implement the OpenGL libretro APIs. It may be possible to use the stock firmware's retroarch instead of MinUI's minarch to run certain cores but that is left as an exercise for the reader.