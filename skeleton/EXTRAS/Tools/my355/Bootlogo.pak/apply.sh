#!/bin/sh
set -x

export PATH=/tmp/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

DIR="$(cd "$(dirname "$0")" && pwd)"
LAST_CALL_TIME=0
hide() {
	killall show.elf || true
}
show() {
    CURRENT_TIME=$(date +%s)
    if [ $((CURRENT_TIME - LAST_CALL_TIME)) -lt 2 ]; then
		DELAY=$((2 - (CURRENT_TIME - LAST_CALL_TIME)))
		echo "delay for $DELAY seconds"
        sleep $DELAY
    fi
    
    hide
    show.elf $DIR/res/$1 300 &
    LAST_CALL_TIME=$(date +%s)
}

show "prep-env.png"
echo "preparing environment"
cd $(dirname "$0")
cp -r payload/* /tmp
cd /tmp

show "extract-img.png"
echo "extracting boot.img"
dd if=/dev/mtd2ro of=boot.img bs=131072

show "unpack-img.png"
echo "unpacking boot.img"
mkdir -p bootimg
unpackbootimg -i boot.img -o bootimg

show "unpack-res.png"
echo "unpacking resources"
mkdir -p bootres
cp bootimg/boot.img-second bootres/
cd bootres
rsce_tool -u boot.img-second

show "replace-logo.png"
echo "replacing logo"
cp -f ../logo.bmp ./
cp -f ../logo.bmp ./logo_kernel.bmp

show "pack-res.png"
echo "packing updated resources"
for file in *; do
    [ "$(basename "$file")" != "boot.img-second" ] && set -- "$@" -p "$file"
done
rsce_tool "$@"

show "pack-img.png"
echo "packing updated boot.img"
cp -f boot-second ../bootimg
cd ../
rm boot.img
mkbootimg --kernel bootimg/boot.img-kernel --second bootimg/boot-second --base 0x10000000 --kernel_offset 0x00008000 --ramdisk_offset 0xf0000000 --second_offset 0x00f00000 --pagesize 2048 --hashtype sha1 -o boot.img

show "flash-img.png"
echo "flashing updated boot.img"
flashcp boot.img /dev/mtd2 && sync

show "reboot.png"
echo "done, rebooting"

sleep 2

# self-destruct
mv $DIR $DIR.disabled
reboot

exit