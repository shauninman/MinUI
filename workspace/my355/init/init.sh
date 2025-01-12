#!/bin/sh
# will be copied to /miyoo355/app/355/ during build

set -x

if [ -f /usr/miyoo/bin/runmiyoo-original.sh ]; then
	echo "already installed"
	exit 0
fi

DIR="$(cd "$(dirname "$0")" && pwd)"

export PATH=/tmp/bin:$DIR/payload/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$DIR/payload/lib:$LD_LIBRARY_PATH

LAST_CALL_TIME=0
hide() {
	# killall show.elf || true
	touch /tmp/fbdisplay_exit
}
show() {
    CURRENT_TIME=$(date +%s)
    if [ $((CURRENT_TIME - LAST_CALL_TIME)) -lt 2 ]; then
		DELAY=$((2 - (CURRENT_TIME - LAST_CALL_TIME)))
		echo "delay for $DELAY seconds"
        sleep $DELAY
    fi
    
    hide
    # show.elf $DIR/res/$1 300 &
    /usr/bin/fbdisplay $DIR/res/$1 &
    LAST_CALL_TIME=$(date +%s)
}

show "prep-env.png"
echo "preparing environment"
cd "$DIR"
cp -r payload/* /tmp
cd /tmp

show "extract-root.png"
echo "extracting rootfs"
dd if=/dev/mtd3ro of=old_rootfs.squashfs bs=131072

show "unpack-root.png"
echo "unpacking rootfs"
unsquashfs old_rootfs.squashfs

show "inject-hook.png"
echo "swapping runmiyoo.sh"
mv squashfs-root/usr/miyoo/bin/runmiyoo.sh squashfs-root/usr/miyoo/bin/runmiyoo-original.sh
mv runmiyoo.sh squashfs-root/usr/miyoo/bin/

show "pack-root.png"
echo "packing updated rootfs"
mksquashfs squashfs-root new_rootfs.squashfs -comp gzip -b 131072 -noappend -exports -all-root -force-uid 0 -force-gid 0

# mount so reboot remains available
mkdir -p /tmp/rootfs
mount /tmp/new_rootfs.squashfs /tmp/rootfs
export PATH=/tmp/rootfs/bin:/tmp/rootfs/usr/bin:/tmp/rootfs/sbin:$PATH
export LD_LIBRARY_PATH=/tmp/rootfs/lib:/tmp/rootfs/usr/lib:$LD_LIBRARY_PATH

show "flash-root.png"
echo "flashing updated rootfs"
flashcp new_rootfs.squashfs /dev/mtd3 && sync

show "reboot.png"
echo "done, rebooting"
sleep 2
reboot
while :; do
	sleep 1
done
exit
