#!/bin/sh

DIR="$SDCARD_PATH/Tools/$PLATFORM/Splore.pak/"
PLUS_DIR="/mnt/sdcard/Tools/rg35xxplus/Splore.pak/pico-8"
CUBE_DIR="/mnt/sdcard/Tools/rg40xxcube/Splore.pak/pico-8"
RGB30_DIR="/mnt/sdcard/Tools/rgb30/Splore.pak/pico-8"

launch_cart() {
	./pico8_64 -run "$1" -joystick 0 -root_path "/mnt/sdcard/Roms/Pico-8 (P8-NATIVE)"
}

cd "$DIR"

if [ ! -d "$RGB30_DIR" ]; then
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
elif [ ! -f "$RGB30_DIR/sdl_controllers.txt" ]; then
	cp "$DIR/sdl_controllers.txt" "$RGB30_DIR";
fi

# ensure correct sdl controller file is in place
cmp -s "$DIR/sdl_controllers.txt" "$RGB30_DIR/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp ./sdl_controllers.txt ./pico-8;
fi

# try launching from various locations the P8 files might live
if [ -f "$RGB30_DIR/pico8_64" ]; then
	cd "$RGB30_DIR" && launch_cart "$1"
elif [ -f "$CUBE_DIR/pico8_64" ]; then
	cd "$CUBE_DIR" && launch_cart "$1"
elif [ -f "$PLUS_DIR/pico8_64" ]; then
	cd "$PLUS_DIR" && launch_cart "$1"
else
	show.elf "$DIR/missing.png" 4
fi
