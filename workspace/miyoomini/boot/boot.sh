#!/bin/sh

# TODO: copy boot to build/BASE/ as .tmp_update and rename boot.sh to updater

# install
if [ -d "/mnt/SDCARD/miyoo" ] ; then
	exit 0
fi

# or launch (and keep launched)
LAUNCH_PATH="/mnt/SDCARD/.system/miyoomini/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH" >> $A_LOG
done

reboot # under no circumstances should stock be allowed to touch this card