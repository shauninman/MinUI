#!/bin/sh

DIR="$(dirname "$0")"
PICO8_DIR="$DIR/pico-8"
PLUS_DIR="/mnt/sdcard/Tools/rg35xxplus/Splore.pak/pico-8"
CUBE_DIR="/mnt/sdcard/Tools/rg40xxcube/Splore.pak/pico-8"
RGB30_DIR="/mnt/sdcard/Tools/rgb30/Splore.pak/pico-8"

launch_splore() {
	./pico8_dyn -splore -joystick 0 -root_path "/mnt/sdcard/Roms/Pico-8 (P8-NATIVE)"
}

if [ ! -d "$PLUS_DIR" ] && [ ! -d "$CUBE_DIR" ] && [ ! -d "$RGB30_DIR" ]; then
	show.elf "$DIR/missing.png" 4
	exit
# add sdl controller file if not present in pico-8 folder
elif [ ! -f "$PICO8_DIR/sdl_controllers.txt" ]; then
	cp "$DIR/sdl_controllers.txt" "$PICO8_DIR";
fi

# ensure correct sdl controller file is in place
cmp -s "$DIR/sdl_controllers.txt" "$PICO8_DIR/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp ./sdl_controllers.txt ./pico-8;
fi

# put haptic-capable SDL2 and cheater's wget in P8's paths
export LD_LIBRARY_PATH="$DIR:$LD_LIBRARY_PATH"
export PATH="$DIR:$PATH"

# try launching from various locations the P8 files might live,
# starting with the current platform
if [ -f "$PICO8_DIR/pico8_dyn" ]; then
	cd "$PICO8_DIR" && launch_splore
elif [ -f "$PLUS_DIR/pico8_dyn" ]; then
	cd "$PLUS_DIR" && launch_splore
elif [ -f "$CUBE_DIR/pico8_dyn" ]; then
	cd "$CUBE_DIR" && launch_splore
elif [ -f "$RGB30_DIR/pico8_dyn" ]; then
	cd "$RGB30_DIR" && launch_splore
else
	show.elf "$DIR/missing.png" 4
fi
