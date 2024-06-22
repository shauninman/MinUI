#!/bin/sh

HOME=$USERDATA_PATH/splore
mkdir -p $HOME

# cleanup beta puke
if [ -d $USERDATA_PATH/carts ]; then
	cd $USERDATA_PATH
	mv activity_log.txt $HOME
	mv backup $HOME
	mv bbs $HOME
	mv carts $HOME
	mv cdata $HOME
	mv config.txt $HOME
	mv cstore $HOME
	mv log.txt $HOME
	mv plates $HOME
	mv plates $HOME
	mv sdl_controllers.txt $HOME
fi

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

# ensure correct sdl controller file is in place
cmp -s "$DIR/sdl_controllers.txt" "$DIR/pico-8/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp ./sdl_controllers.txt ./pico-8;
fi

cd ./pico-8 && ./pico8_64 -splore -pixel_perfect 1 -joystick 0
