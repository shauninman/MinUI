#!/bin/sh

SDCARD_PATH=/mnt/SDCARD
SWAP=$SDCARD_PATH/.userdata/trimui/swapfile
if [ ! -f "$SWAP" ]; then
	show.elf $SDCARD_PATH/.system/trimui/dat/swap.png
	dd if=/dev/zero of=$SWAP bs=1M count=128
	chmod 600 $SWAP
	mkswap $SWAP
fi
swapon $SWAP

touch /tmp/using-swap