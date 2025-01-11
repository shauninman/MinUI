#!/bin/sh

# becomes /usr/miyoo/bin/runmiyoo.sh on my355

#wait for sdcard mounted
mounted=`cat /proc/mounts | grep SDCARD`
cnt=0
while [ "$mounted" == "" ] && [ $cnt -lt 6 ] ; do
   sleep 0.5
   cnt=`expr $cnt + 1`
   mounted=`cat /proc/mounts | grep SDCARD`
done

UPDATER_PATH=/mnt/SDCARD/.tmp_update/updater
if [ -f "$UPDATER_PATH" ]; then
	"$UPDATER_PATH"
else
	/usr/miyoo/bin/runmiyoo-original.sh
fi
