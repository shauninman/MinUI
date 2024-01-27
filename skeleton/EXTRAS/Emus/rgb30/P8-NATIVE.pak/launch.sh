#!/bin/sh

DIR=$(dirname "$0")
PICO8_DIR="$SDCARD_PATH/Tools/$PLATFORM/Splore.pak/pico-8"

if [ ! -d "$PICO8_DIR" ]; then
	show.elf "$DIR/missing.png" 4
	exit
fi

cd "$PICO8_DIR"
./pico8_64 -run "$1" -pixel_perfect 1 -joystick 0

