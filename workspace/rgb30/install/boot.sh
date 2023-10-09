#!/bin/sh
# NOTE: becomes .tmp_update/rgb30.sh

PLATFORM="rgb30"
SDCARD_PATH="/storage/roms"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"
SYSTEM_DIR="$(dirname "$0")/$PLATFORM"

echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor

# copy firmware update from TF2 to TF1 and reboot
FW_PATH=$(ls -1 $SDCARD_PATH/JELOS*.tar 2>/dev/null | head -n 1)
if [[ ! -z "$FW_PATH" && -f "$FW_PATH" ]]; then
	ply-image $SYSTEM_DIR/firmware.png
	mv $FW_PATH /storage/.update && sync && cat /dev/zero > /dev/fb0 && reboot
fi

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	if [ -d "$SYSTEM_PATH" ]; then
		ply-image $SYSTEM_DIR/updating.png
	else
		ply-image $SYSTEM_DIR/installing.png
	fi
	
	unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH" # &> $SDCARD_PATH/unzip.txt
	rm -f "$UPDATE_PATH"
	
	cat /dev/zero > /dev/fb0
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
