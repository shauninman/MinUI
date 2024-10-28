#!/bin/sh
# NOTE: becomes .tmp_update/tg3040.sh

PLATFORM="tg3040"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

# CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# echo performance > "$CPU_PATH"

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	echo ok
	export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
	export PATH=/usr/trimui/bin:$PATH

	# leds_off
	echo 0 > /sys/class/led_anim/max_scale
	echo 0 > /sys/class/led_anim/max_scale_lr
	echo 0 > /sys/class/led_anim/max_scale_f1f2

	cd $(dirname "$0")/$PLATFORM
	if [ -d "$SYSTEM_PATH" ]; then
		./show.elf ./updating.png
	else
		./show.elf ./installing.png
	fi

	./unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH" # &> /mnt/SDCARD/unzip.txt
	rm -f "$UPDATE_PATH"

	# the updated system finishes the install/update
	# $SYSTEM_PATH/$PLATFORM/bin/install.sh # &> $SDCARD_PATH/install.txt
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
