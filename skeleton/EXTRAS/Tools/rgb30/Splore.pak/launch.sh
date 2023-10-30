#!/bin/sh

DIR="$(dirname "$0")"
cd "$DIR"

if [ ! -d ./pico-8 ]; then
	PICO8_ZIP=$(ls -1 ./pico-8*raspi.zip 2>/dev/null | head -n 1)
	if [[ ! -z "$PICO8_ZIP" && -f "$PICO8_ZIP" ]]; then
		show.elf "$DIR/extracting.png" 60 &
		unzip -o "$PICO8_ZIP" -d ./
		cp ./sdl_controllers.txt ./pico-8
		killall -s KILL show.elf
	else
		show.elf "$DIR/missing.png" 4
		exit
	fi
fi

cd ./pico-8 && ./pico8_64 -splore -pixel_perfect 1 -joystick 0