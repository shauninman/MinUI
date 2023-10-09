#!/bin/sh
# NOTE: becomes .tmp_update/rgb30.sh

PLATFORM="rgb30"
SDCARD_PATH="/storage/roms"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

rm $SDCARD_PATH/FSCK*

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
	
	DIR="$(dirname "$0")/$PLATFORM"
	if [ -d "$SYSTEM_PATH" ]; then
		ply-image $DIR/updating.png
	else
		ply-image $DIR/installing.png
	fi
	
	unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH" # &> $SDCARD_PATH/unzip.txt
	rm -f "$UPDATE_PATH"
	
	cat /dev/zero > /dev/fb0
fi

# TODO: will this loop break shutdown?
LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
