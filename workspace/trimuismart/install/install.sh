#!/bin/sh

# NOTE: becomes trimui/app/MainUI

SDCARD_PATH=/mnt/SDCARD
cd "$SDCARD_PATH/trimui/app"

if [ ! -f /etc/main.original ]; then
	cp /etc/main /etc/main.original
	cp ./main.sh /etc/main
fi

# .tmp_update/updater does the actual installation (and later, updating)
cp -rf .tmp_update $SDCARD_PATH/
rm -rf "$SDCARD_PATH/trimui"
sync
$SDCARD_PATH/.tmp_update/updater

poweroff # under no circumstances should stock be allowed to touch this card
