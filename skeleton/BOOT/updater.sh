#!/bin/sh

# NOTE: becomes .tmp_update/updater

INFO=`cat /proc/cpuinfo 2> /dev/null`
case $INFO in
*"Allwinner"*)
	PLATFORM="trimui"
	;;
*"sun8i"*)
	PLATFORM="trimuismart"
	;;
*"SStar"*)
	PLATFORM="miyoomini"
	;;
esac

/mnt/SDCARD/.tmp_update/$PLATFORM.sh

# force shutdown so nothing can modify the SD card
echo s > /proc/sysrq-trigger
echo u > /proc/sysrq-trigger
echo o > /proc/sysrq-trigger