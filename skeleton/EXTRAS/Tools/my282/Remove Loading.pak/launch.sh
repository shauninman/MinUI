#!/bin/sh

overclock.elf performance 2 1200 384 1080 0

DIR="$(dirname "$0")"
cd "$DIR"

{

# copy to tmp to get around spaces in lib path
# these are sourced from the my282 toolchain buildroot
cp -r bin /tmp
cp -r lib /tmp

export PATH=/tmp/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

show.elf "$DIR/patch.png" 600 &

cd /tmp

rm -rf rootfs squashfs-root rootfs.modified

cp /dev/mtdblock3 rootfs

unsquashfs rootfs
if [ $? -ne 0 ]; then
	killall -9 show.elf
	show.elf "$DIR/abort.png" 2
	sync
	exit 1
fi

BOOT_PATH=/tmp/squashfs-root/etc/init.d/boot

sed -i '/^#added by cb.*/,/^echo "show loading txt" >> \/tmp\/.show_loading_txt.log/d' $BOOT_PATH
echo "patched $BOOT_PATH" 

mksquashfs squashfs-root rootfs.modified -comp xz -b 256k
if [ $? -ne 0 ]; then
	killall -9 show.elf
	show.elf "$DIR/abort.png" 2
	sync
	exit 1
fi

mtd write /tmp/rootfs.modified rootfs
killall -9 show.elf

} &> ./log.txt

mv "$DIR" "$DIR.disabled"