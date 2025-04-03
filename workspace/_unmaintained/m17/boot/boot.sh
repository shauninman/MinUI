#!/bin/sh

# NOTE: becomes em_ui.sh

PLATFORM="m17"
SDCARD_PATH="/sdcard"
UPDATE_PATH="$SDCARD_PATH/NextUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
	else
		ACTION=updating
	fi

	# initialize fb0
	cat /sys/class/graphics/fb0/modes > /sys/class/graphics/fb0/mode

	# extract the zip file appended to the end of this script to tmp
	CUT=$((`grep -n '^BINARY' $0 | cut -d ':' -f 1 | tail -1` + 1))
	tail -n +$CUT "$0" | uudecode -o /tmp/data
	
	# unzip and display one of the two images it contains 
	unzip -o /tmp/data -d /tmp
	dd if=/tmp/$ACTION of=/dev/fb0
	sync
	
	# finally unzip NextUI.zip
	unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH"
	rm -f "$UPDATE_PATH"
	sync
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/$PLATFORM/bin/install.sh # &> $SDCARD_PATH/install.txt
	dd if=/dev/zero of=/dev/fb0
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/NextUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	taskset 8 "$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card

exit 0

