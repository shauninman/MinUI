#!/bin/sh

# SYSTEM/usr/bin/autostart.sh

SDCARD_PATH=/storage/TF2
SYSTEM_FRAG=/.system/magicmini
UPDATE_FRAG=/MinUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

# is there an update available?
if [ -f $UPDATE_PATH ]; then
	echo "zip detected" >> $SDCARD_PATH/log.txt
	
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		echo "install MinUI" >> $SDCARD_PATH/log.txt
	else
		ACTION=updating
		echo "update MinUI" >> $SDCARD_PATH/log.txt
	fi
	
	# show action
	dd if=/usr/config/minui/$ACTION.bmp of=/dev/fb0 bs=71 skip=1
	echo 0,0 > /sys/class/graphics/fb0/pan

	unzip -o $UPDATE_PATH -d $SDCARD_PATH >> $SDCARD_PATH/log.txt
	rm -f $UPDATE_PATH

	# the updated system finishes the install/update
	if [ -f $SYSTEM_PATH/bin/install.sh ]; then
		$SYSTEM_PATH/bin/install.sh >> $SDCARD_PATH/log.txt
	fi
fi

LAUNCH_PATH=$SYSTEM_PATH/paks/MinUI.pak/launch.sh
if [ -f $LAUNCH_PATH ]; then
	$LAUNCH_PATH
fi

sync && shutdown now
