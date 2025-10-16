#!/bin/sh

overclock.elf $CPU_SPEED_GAME # slow down, my282 didn't like overclock during this operation

DIR="$(dirname "$0")"
cd "$DIR"

{

# squashfs tools and liblzma.so sourced from toolchain buildroot
cp -r bin /tmp
cp -r lib /tmp

export PATH=/tmp/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

show.elf "$DIR/patch.png" 600 &

cd /tmp

rm -rf customer squashfs-root customer.modified

cp /dev/mtd6 customer

unsquashfs customer
if [ $? -ne 0 ]; then
	killall -9 show.elf
	show.elf "$DIR/abort.png" 2
	sync
	exit 1
fi

sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/main
echo "patched main"

mksquashfs squashfs-root customer.mod -comp xz -b 131072 -xattrs -all-root
if [ $? -ne 0 ]; then
	killall -9 show.elf
	show.elf "$DIR/abort.png" 2
	sync
	exit 1
fi

dd if=customer.mod of=/dev/mtdblock6 bs=128K conv=fsync

killall -9 show.elf

} &> ./log.txt

mv "$DIR" "$DIR.disabled"
reboot
