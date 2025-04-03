#!/system/bin/sh

/usbdbg.sh device

TF1_PATH=/mnt/mmc # ROMS partition
TF2_PATH=/mnt/sdcard
SDCARD_PATH=$TF1_PATH
SYSTEM_DIR=/.system
SYSTEM_FRAG=$SYSTEM_DIR/rg35xx
UPDATE_FRAG=/NextUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

mkdir /mnt/sdcard
if [ -e /dev/block/mmcblk1p1 ]; then
	SDCARD_DEVICE=/dev/block/mmcblk1p1
else
	SDCARD_DEVICE=/dev/block/mmcblk1
fi
mount -t vfat -o rw,utf8,noatime $SDCARD_DEVICE /mnt/sdcard
if [ $? -ne 0 ]; then
	mount -t exfat -o rw,utf8,noatime $SDCARD_DEVICE /mnt/sdcard
	if [ $? -ne 0 ]; then
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

if [ -d ${TF1_PATH}${SYSTEM_FRAG} ] || [ -f ${TF1_PATH}${UPDATE_FRAG} ]; then
	if [ ! -L $TF2_PATH ]; then
		# .system found on TF1 but TF2 is present
		# so unmount and symlink to TF1 path
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
	FLAG_PATH=/misc/.minstalled
	if [ ! -f $FLAG_PATH ]; then
		ACTION=installing
	else
		ACTION=updating
	fi

	# extract the zip file appended to the end of this script to tmp
	# and display one of the two images it contains 
	CUT=$((`busybox grep -n '^BINARY' $0 | busybox cut -d ':' -f 1 | busybox tail -1` + 1))
	busybox tail -n +$CUT "$0" | busybox uudecode -o /tmp/data
	busybox unzip -o /tmp/data -d /tmp
	busybox fbset -g 640 480 640 480 16
	dd if=/tmp/$ACTION of=/dev/fb0
	sync
	
	busybox unzip -o $UPDATE_PATH -d $SDCARD_PATH
	rm -f $UPDATE_PATH
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/bin/install.sh # &> $SDCARD_PATH/install.txt
fi

ROOTFS_IMAGE=$SYSTEM_PATH/rootfs.ext2
if [ ! -f $ROOTFS_IMAGE ]; then
	# fallback to stock demenu.bin, based on dmenu_ln
	ACT="/tmp/.next"
	CMD="/mnt/vendor/bin/dmenu.bin"
	touch "$ACT"
	while [ -f $CMD ]; do
		if $CMD; then
			if [ -f "$ACT" ]; then
				if  ! sh $ACT; then
					echo
				fi
				rm -f "$ACT"
			fi
		fi
	done
	sync && reboot -p
fi

ROOTFS_MOUNTPOINT=/cfw
LOOPDEVICE=/dev/block/loop7
mkdir $ROOTFS_MOUNTPOINT
busybox losetup $LOOPDEVICE $ROOTFS_IMAGE
mount -r -w -o loop -t ext4 $LOOPDEVICE $ROOTFS_MOUNTPOINT
rm -rf $ROOTFS_MOUNTPOINT/tmp/*
mkdir $ROOTFS_MOUNTPOINT/mnt/mmc
mkdir $ROOTFS_MOUNTPOINT/mnt/sdcard
for f in dev dev/pts proc sys mnt/mmc mnt/sdcard # tmp doesn't work for some reason?
do
	mount -o bind /$f $ROOTFS_MOUNTPOINT/$f
done

export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH
export LD_LIBRARY_PATH=/usr/lib/:/lib/
export HOME=$SDCARD_PATH
busybox chroot $ROOTFS_MOUNTPOINT $SYSTEM_PATH/paks/NextUI.pak/launch.sh

umount $ROOTFS_MOUNTPOINT
busybox losetup --detach $LOOPDEVICE
sync && reboot -p

exit 0

