#!/bin/sh

cd $(dirname "$0")

export PATH=/usr/magicx/bin:/usr/bin:/usr/sbin:/bin:/sbin
export LD_LIBRARY_PATH=/usr/magicx/lib:/usr/lib:/lib

SDCARD_PATH=/mnt/SDCARD

# .tmp_update/updater does the actual installation (and later, updating)
cp -rf .tmp_update $SDCARD_PATH/
rm -rf "$SDCARD_PATH/magicx"
sync
$SDCARD_PATH/.tmp_update/updater

# under no circumstances should stock be allowed to touch this card
poweroff
