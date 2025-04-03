#!/bin/sh

rm -f "$USERDATA_PATH/mstick.bin"

DIR="$(dirname "$0")"
cd "$DIR"
show.elf "$DIR/calibrate.png" 2