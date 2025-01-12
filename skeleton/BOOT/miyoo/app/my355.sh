#!/bin/sh

cd $(dirname "$0")

SDCARD_PATH=/mnt/SDCARD
MIYOO_DIR=miyoo355

if [ ! -f /usr/miyoo/bin/runmiyoo-original.sh ]; then
	if [ -d $SDCARD_PATH/.tmp_update/ ]; then
		rm -rf $SDCARD_PATH/.tmp_update/
	fi
	my355/init.sh
fi

export PATH=/usr/miyoo/bin:/usr/miyoo/sbin:/usr/bin:/usr/sbin:/bin:/sbin
export LD_LIBRARY_PATH=/usr/miyoo/lib:/usr/lib:/lib


# .tmp_update/updater does the actual installation (and later, updating)
cp -rf .tmp_update $SDCARD_PATH/
rm -rf "$SDCARD_PATH/$MIYOO_DIR"
sync
$SDCARD_PATH/.tmp_update/updater

# under no circumstances should stock be allowed to touch this card
poweroff
