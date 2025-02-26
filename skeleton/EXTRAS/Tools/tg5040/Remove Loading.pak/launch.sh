#!/bin/sh

DIR="$(dirname "$0")"
cd "$DIR"

sed -i '/^\/usr\/sbin\/pic2fb \/etc\/splash.png/d' /etc/init.d/runtrimui
show.elf "$DIR/$DEVICE/done.png" 2

mv "$DIR" "$DIR.disabled"