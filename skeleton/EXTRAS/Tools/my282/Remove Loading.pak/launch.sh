#!/bin/sh

DIR="$(dirname "$0")"
cd "$DIR"

# overclock for squashfs tools
overclock.elf userspace 4 1512 384 1080 0

{

# copy to tmp to get around spaces in path
# these are source from the my282 toolchain buildroot
cp -r bin /tmp
cp -r lib /tmp

export PATH=/tmp/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

show.elf "$DIR/patch.png" 600 &

cd /tmp
cp /dev/mtdblock3 rootfs
unsquashfs rootfs

BOOT_PATH=/tmp/squashfs-root/etc/init.d/boot

sed -i '/^#added by cb.*/,/^echo "show loading txt" >> \/tmp\/.show_loading_txt.log/d' $BOOT_PATH

mksquashfs squashfs-root rootfs.modified -comp xz -b 256k

mtd write /tmp/rootfs.modified rootfs
killall -9 show.elf

} &> ./log.txt

mv "$DIR" "$DIR.disabled"