#!/bin/sh

# becomes /.system/magicmini/bin/install.sh

DTB_NAME=rk3562-magicx-linux.dtb
OLD_DTB=/flash/$DTB_NAME
NEW_DTB=/storage/TF2/.system/magicmini/dat/$DTB_NAME

OLD_SUM=$(md5sum "$OLD_DTB" | awk '{ print $1 }')
NEW_SUM=$(md5sum "$NEW_DTB" | awk '{ print $1 }')

if [ "$OLD_SUM" != "$NEW_SUM" ]; then
	echo "updating dtb"
	mount -o remount,rw /flash
	cp "$NEW_DTB" "$OLD_DTB"
	mount -o remount,ro /flash
	shutdown -r now
fi

