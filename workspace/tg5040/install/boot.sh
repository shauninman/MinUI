#!/bin/sh
# NOTE: becomes .tmp_update/tg5040.sh

PLATFORM="tg5040"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
PAKZ_PATH="$SDCARD_PATH/*.pakz"
SYSTEM_PATH="$SDCARD_PATH/.system"

echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=2000000
echo $CPU_SPEED_PERF > $CPU_PATH

##Remove Old Led Daemon
if [ -f "/etc/LedControl" ]; then
	rm -Rf "/etc/LedControl"
fi
if [ -f "/etc/init.d/lcservice" ]; then
	/etc/init.d/lcservice disable
	rm /etc/init.d/lcservice
fi

export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
export PATH=/usr/trimui/bin:$PATH

TRIMUI_MODEL=`strings /usr/trimui/bin/MainUI | grep ^Trimui`
if [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
	DEVICE="brick"
fi

# leds_off
echo 0 > /sys/class/led_anim/max_scale

# generic NextUI package install
for pakz in $PAKZ_PATH; do
	if [ ! -e "$pakz" ]; then continue; fi
	echo "Installing $pakz"
	cd $(dirname "$0")/$PLATFORM
	./show.elf ./$DEVICE/installing.png

	./unzip -o -d "$SDCARD_PATH" "$pakz" # >> $pakz.txt
	rm -f "$pakz"

	# run postinstall if present
	if [ -f $SDCARD_PATH/post_install.sh ]; then
		$SDCARD_PATH/post_install.sh # > $pakz_post.txt
		rm -f $SDCARD_PATH/post_install.sh
	fi
done

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	echo ok
	cd $(dirname "$0")/$PLATFORM
	if [ -d "$SYSTEM_PATH" ]; then
		./show.elf ./$DEVICE/updating.png
	else
		./show.elf ./$DEVICE/installing.png
	fi

	# clean replacement for core paths
	rm -rf $SYSTEM_PATH/$PLATFORM/bin
	rm -rf $SYSTEM_PATH/$PLATFORM/lib
	rm -rf $SYSTEM_PATH/$PLATFORM/paks/MinUI.pak

	./unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH" # &> /mnt/SDCARD/unzip.txt
	rm -f "$UPDATE_PATH"

	# the updated system finishes the install/update
	if [ -f $SYSTEM_PATH/$PLATFORM/bin/install.sh ]; then
		$SYSTEM_PATH/$PLATFORM/bin/install.sh # &> $SDCARD_PATH/log.txt
	fi
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
if [ -f "$LAUNCH_PATH" ] ; then
	"$LAUNCH_PATH"
fi
killall trimui_inputd

poweroff # under no circumstances should stock be allowed to touch this card
while true
do
	echo "Waiting for poweroff."
	sleep 1
done