#!/bin/sh

TF1_PATH=/mnt/mmc # TF1/NO NAME partition
TF2_PATH=/mnt/sdcard # TF2
SDCARD_PATH=$TF1_PATH
SYSTEM_DIR=/.system
SYSTEM_FRAG=$SYSTEM_DIR/rg35xxplus
UPDATE_FRAG=/MinUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

rm $TF1_PATH/log.txt
touch $TF1_PATH/log.txt

if mountpoint -q $TF2_PATH; then
	echo "TF2 already mounted" >> $TF1_PATH/log.txt
else
	echo "mount TF2" >> $TF1_PATH/log.txt
	mkdir -p $TF2_PATH
	SDCARD_DEVICE=/dev/mmcblk1p1
	mount -t vfat -o rw,utf8,noatime $SDCARD_DEVICE $TF2_PATH
	if [ $? -ne 0 ]; then
		echo "mount failed, symlink to TF1" >> $TF1_PATH/log.txt
		rm -d $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
	
fi

if [ -d ${TF1_PATH}${SYSTEM_FRAG} ] || [ -f ${TF1_PATH}${UPDATE_FRAG} ]; then
	echo "found MinUI on TF1" >> $TF1_PATH/log.txt
	if [ ! -L $TF2_PATH ]; then
		echo "no system on TF2, unmount and symlink to TF1" >> $TF1_PATH/log.txt
		umount $TF2_PATH
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

SDCARD_PATH=$TF2_PATH
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

# is there an update available?
if [ -f $UPDATE_PATH ]; then
	echo "zip detected" >> $TF1_PATH/log.txt
	
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		echo "install MinUI" >> $TF1_PATH/log.txt
	else
		ACTION=updating
		echo "update MinUI" >> $TF1_PATH/log.txt
	fi
	
	# extract tar.gz from this sh file
	LINE=$((`grep -na '^BINARY' $0 | cut -d ':' -f 1 | tail -1` + 1))
	tail -n +$LINE "$0" > /tmp/data
	tar -xf /tmp/data -C /tmp > /dev/null 2>&1
	
	# show action
	dd if=/tmp/$ACTION of=/dev/fb0
	sync
	echo 0,0 > /sys/class/graphics/fb0/pan
	
	# install bootlogo.bmp
	if [ $ACTION = "installing" ]; then
		BOOT_DEVICE=/dev/mmcblk0p2
		BOOT_PATH=/mnt/boot
		mkdir -p $BOOT_PATH
		mount -t vfat -o rw,utf8,noatime $BOOT_DEVICE $BOOT_PATH
		cp /tmp/bootlogo.bmp $BOOT_PATH
		umount $BOOT_PATH
		rm -rf $BOOT_PATH
	fi
	
	/tmp/unzip -o $UPDATE_PATH -d $SDCARD_PATH
	rm -f $UPDATE_PATH
		
	cd /tmp
	rm data installing updating bootlogo.bmp unzip
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/bin/install.sh # &> $SDCARD_PATH/install.txt
fi

# while :; do; sleep 5; done

if [ -f $SYSTEM_PATH/paks/MinUI.pak/launch.sh ]; then
	echo "launch MinUI" >> $TF1_PATH/log.txt
	$SYSTEM_PATH/paks/MinUI.pak/launch.sh > $SDCARD_PATH/log.txt 2>&1
else
	echo "couldn't find launch.sh" >> $TF1_PATH/log.txt
	ls $SDCARD_PATH >> $TF1_PATH/log.txt
fi

sync && poweroff

exit 0
