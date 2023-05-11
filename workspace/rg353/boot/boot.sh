#!/bin/sh

# becomes initrd/rcS
# which then becomes /etc/init.d/rcS
# when anbernic squashfs becomes root

cd $(dirname "$0")

SDCARD_PATH=/mnt/SDCARD
SYSTEM_DIR=/.system
SYSTEM_FRAG=$SYSTEM_DIR/rg353
UPDATE_FRAG=/MinUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

mkdir /mnt/SDCARD
mount -t debugfs debugfs /sys/kernel/debug/
if [ -e /dev/mmcblk2p1 ]; then
	SDCARD_DEVICE=/dev/mmcblk2p1
else
	SDCARD_DEVICE=/dev/mmcblk1p3
fi
mount -t vfat -o rw,utf8,noatime $SDCARD_DEVICE /mnt/SDCARD

ROOTFS_IMAGE=$SYSTEM_PATH/rootfs.ext2
ROOTFS_MOUNTPOINT=/cfw
LOOPDEVICE=/dev/loop7
mkdir $ROOTFS_MOUNTPOINT
losetup $LOOPDEVICE $ROOTFS_IMAGE
mount -r -w -o loop -t ext4 $LOOPDEVICE $ROOTFS_MOUNTPOINT
rm -rf $ROOTFS_MOUNTPOINT/tmp/*
mkdir $ROOTFS_MOUNTPOINT/mnt/SDCARD
for f in boot dev dev/pts proc sys mnt/SDCARD # tmp doesn't work for some reason? neither does boot...because they're already mounts?
do
	mount -o bind /$f $ROOTFS_MOUNTPOINT/$f
done

mkdir -p $ROOTFS_MOUNTPOINT/mnt/BOOT
mount --bind /boot $ROOTFS_MOUNTPOINT/mnt/BOOT

export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH
export LD_LIBRARY_PATH=/usr/lib/:/lib/
export HOME=$SDCARD_PATH
chroot $ROOTFS_MOUNTPOINT $SYSTEM_PATH/paks/MinUI.pak/launch.sh

umount $ROOTFS_MOUNTPOINT
losetup --detach $LOOPDEVICE
sync && reboot -p

exit 0