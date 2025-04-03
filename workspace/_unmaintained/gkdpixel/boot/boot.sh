#!/bin/sh
# becomes /usr/sbin/frontend_start

# Restore the framebuffer to a working state
# Reset the console
# Disactivate the console on framebuffer
/usr/sbin/unlockvt > /dev/null
/usr/bin/reset
echo 0 > /sys/devices/virtual/vtconsole/vtcon1/bind

SDCARD_PATH=/media/roms
SYSTEM_PATH=$SDCARD_PATH/.system/gkdpixel
UPDATE_PATH=$SDCARD_PATH/NextUI.zip

# is there an update available?
if [ -f $UPDATE_PATH ]; then
	echo "zip detected" >> $SDCARD_PATH/log.txt
	
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		echo "install NextUI" >> $SDCARD_PATH/log.txt
	else
		ACTION=updating
		echo "update NextUI" >> $SDCARD_PATH/log.txt
	fi
	
	# show action
	dd skip=54 if=/usr/share/nextui/$ACTION.bmp of=/dev/fb0 bs=1
	sync
	
	unzip -o $UPDATE_PATH -d $SDCARD_PATH
	rm -f $UPDATE_PATH
	
	rm -rf $SDCARD_PATH/gkdpixel
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/bin/install.sh
fi

LAUNCH_PATH=/media/roms/.system/gkdpixel/paks/NextUI.pak/launch.sh
if [ -f "$LAUNCH_PATH" ]; then
	$LAUNCH_PATH > /media/roms/log.txt 2>&1
else
	/usr/sbin/frontend_start.original
fi