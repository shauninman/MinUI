#!/bin/sh

DIR="$(dirname "$0")"
PICO8_DIR="$DIR/pico-8"
PLUS_DIR="/mnt/sdcard/Tools/rg35xxplus/Splore.pak/pico-8"
CUBE_DIR="/mnt/sdcard/Tools/rg40xxcube/Splore.pak/pico-8"
RGB30_DIR="/mnt/sdcard/Tools/rgb30/Splore.pak/pico-8"

launch_splore() {
	./pico8_64 -splore -joystick 0 -root_path "/mnt/sdcard/Roms/Pico-8 (P8-NATIVE)"
}

cd "$DIR"

if [ ! -d "$PICO8_DIR" ]; then
	PICO8_ZIP=$(ls -1 ./pico-8*raspi.zip 2>/dev/null | head -n 1)
	if [[ ! -z "$PICO8_ZIP" && -f "$PICO8_ZIP" ]]; then
		show.elf "$DIR/extracting.png" 60 &
		unzip -o "$PICO8_ZIP" -d ./
		cp ./sdl_controllers.txt ./pico-8
		killall -s KILL show.elf
	elif [ ! -d "$PLUS_DIR" ] && [ ! -d "$CUBE_DIR" ]; then
		show.elf "$DIR/missing.png" 4
		exit
	fi
# add sdl controller file if not present in pico-8 folder
elif [ ! -f "$PICO8_DIR/sdl_controllers.txt" ]; then
	cp "$DIR/sdl_controllers.txt" "$PICO8_DIR";
fi

# ensure correct sdl controller file is in place
cmp -s "$DIR/sdl_controllers.txt" "$DIR/pico-8/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp ./sdl_controllers.txt ./pico-8;
fi

# try launching from various locations the P8 files might live
if [ -f "$PLUS_DIR/pico8_64" ]; then
	cd "$PLUS_DIR" && launch_splore
elif [ -f "$CUBE_DIR/pico8_64" ]; then
	cd "$CUBE_DIR" && launch_splore
elif [ -f "$RGB30_DIR/pico8_64" ]; then
	cd "$RGB30_DIR" && launch_splore
else
	show.elf "$DIR/missing.png" 4
fi
