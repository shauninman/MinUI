#!/system/bin/sh

# NOTE: this file is not chrooted so it's using stock's everything!
# {

TF1_PATH=/mnt/mmc
TF2_PATH=/mnt/sdcard # TF1 should be linked to this path if TF2 is missing or doesn't contain our system folder
SYSTEM_PATH=${TF2_PATH}/.system/rg35xx
FLAG_PATH=/misc/.minstalled

echo "installing/updating"

if [ ! -f $FLAG_PATH ]; then
	echo "backing up"
	BAK_PATH=$TF1_PATH/bak
	mkdir -p $BAK_PATH
	cp /misc/boot_logo.bmp.gz $BAK_PATH
fi

was_updated() {
	# intiial releases didn't install this properly :sob:
	if [ ! -f /misc/charging.png ]; then
		return 0
	fi
	
	for FILE in /misc/* /misc/*/*; do
		A_PATH=$FILE
		A_NAME=$(busybox basename "$A_PATH")
		B_PATH=$SYSTEM_PATH/dat/$A_NAME
		
		if [[ "$A_NAME" == "boot_logo.bmp.gz" ]]; then
			# we don't care if the user has changed their boot logo
			continue
		fi
		
		if [[ "$A_NAME" == "charging.png" ]]; then
			# we don't care if the user has changed their charging image
			continue
		fi
		
		if [ ! -f "$B_PATH" ]; then
			continue
		fi
	
		A_SUM=$(busybox md5sum $A_PATH | busybox cut -d ' ' -f 1)
		B_SUM=$(busybox md5sum $B_PATH | busybox cut -d ' ' -f 1)
	
		if [[ "$A_SUM" != "$B_SUM" ]]; then
			return 0
		fi
	done
	
	return 1
}

if [ ! -f $FLAG_PATH ] || was_updated; then
	echo "updating misc partition"
	mount -o remount,rw /dev/block/actb /misc
	cp $SYSTEM_PATH/dat/dmenu.bin /misc
	cp $SYSTEM_PATH/dat/ramdisk.img /misc
	
	# boot logo, only installed, never updated
	if [ ! -f $FLAG_PATH ]; then
		cp $SYSTEM_PATH/dat/boot_logo.bmp.gz /misc
	fi
	# charging graphic, only installed, never updated
	if [ ! -f /misc/charging.png ]; then
		cp $SYSTEM_PATH/dat/charging.png /misc
	fi

	touch $FLAG_PATH
	sync && reboot
fi

# } &> /mnt/sdcard/install.txt
