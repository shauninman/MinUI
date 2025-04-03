#!/bin/sh

DIR=$(dirname "$0")
cd $DIR

BOOT_DEV=/dev/mmcblk0p1
BOOT_PATH=/mnt/boot

mkdir -p $BOOT_PATH
mount -t vfat $BOOT_DEV $BOOT_PATH

# convert bootlogo.png -define bmp:format=bmp3 -depth 8 -channel BGRA BMP3:bootlogo.bmp
cp bootlogo.bmp $BOOT_PATH

umount $BOOT_PATH
rm -rf $BOOT_PATH

# self-destruct
mv $DIR $DIR.disabled

reboot