#!/bin/sh

DIR=$(dirname "$0")
PICO8_DIR="$SDCARD_PATH/Tools/$PLATFORM/Splore.pak/pico-8"

if [ ! -d "$PICO8_DIR" ]; then
	show.elf "$DIR/missing.png" 4
	exit
# add sdl controller file if not present in pico-8 folder
elif [ ! -f "$PICO8_DIR/sdl_controllers.txt" ]; then
	cp "$DIR/sdl_controllers.txt" "$PICO8_DIR";
fi

# ensure correct sdl controller file for rgb30 is in use
cmp -s "$DIR/sdl_controllers.txt" "$PICO8_DIR/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp -f "$DIR/sdl_controllers.txt" "$PICO8_DIR";
fi

cd "$PICO8_DIR"
./pico8_64 -run "$1" -pixel_perfect 1 -joystick 0
