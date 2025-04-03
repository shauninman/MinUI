#!/bin/sh
# NOTE: becomes .tmp_update/trimuismart.sh

PLATFORM="trimuismart"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/NextUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > "$CPU_PATH"

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	# TODO: this shouldn't be necessary but might be?
	export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
	export PATH=/usr/trimui/bin:$PATH
	
	cd $(dirname "$0")/$PLATFORM
	./leds_off
	if [ -d "$SYSTEM_PATH" ]; then
		./show.elf ./updating.png
	else
		./show.elf ./installing.png
	fi
	
	./unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH" # &> /mnt/SDCARD/unzip.txt
	rm -f "$UPDATE_PATH"
	
	# the updated system finishes the install/update
	if [ -f $SYSTEM_PATH/$PLATFORM/bin/install.sh ]; then
		$SYSTEM_PATH/$PLATFORM/bin/install.sh # &> $SDCARD_PATH/log.txt
	fi
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/NextUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
