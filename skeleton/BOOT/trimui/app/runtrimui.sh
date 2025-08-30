#!/bin/sh

# becomes /usr/trimui/bin/runtrimui.sh on tg5040/tg3040

MOUNTED=$(grep SDCARD /proc/mounts)
ATTEMPTS=0
while [ -z "$MOUNTED" ] && [ $ATTEMPTS -lt 20 ]; do
	sleep 0.25
	ATTEMPTS=$((ATTEMPTS + 1))
	MOUNTED=$(grep SDCARD /proc/mounts)
done

UPDATER_PATH=/mnt/SDCARD/.tmp_update/updater
if [ -f "$UPDATER_PATH" ]; then
	exec "$UPDATER_PATH"
else
	/usr/trimui/bin/runtrimui-original.sh
fi
