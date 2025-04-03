#!/bin/sh

DIR="/mnt/vendor/bin/fileM"

if [ ! -d "$DIR" ]; then
	DIR="$(dirname "$0")"
	cd "$DIR"
	show.elf "$DIR/missing.png" 4
else
	cd "$DIR"
	HOME="$SDCARD_PATH"
	syncsettings.elf &
	./dinguxCommand_en.dge
fi

