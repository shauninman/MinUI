#!/bin/sh

# TODO: this file is shared with many Trimui and Miyoo devices, which owns it?
# TODO: copy boot folder to build/BASE/ as .tmp_update and rename boot.sh to updater

# TODO: Linux version dffers 
# 	smart	3.4.39
#	mini	4.9.84 
#	models	3.10.65

# VERSION=`cat /proc/version 2> /dev/null`
# case $VERSION in
# *"@alfchen-ubuntu"*)
# 	SYSTEM_NAME="trimui"
# 	;;
# *"@alfchen-NUC"*)
# 	SYSTEM_NAME="miyoomini"
# 	;;
# esac

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