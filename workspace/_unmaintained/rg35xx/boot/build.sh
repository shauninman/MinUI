#!/bin/sh

TARGET=dmenu.bin

mkdir -p output
# TODO: shouldn't this be skipping 54 bytes (the size of the bitmap format header)?
if [ ! -f output/installing ]; then
	dd skip=64 iflag=skip_bytes if=installing.bmp of=output/installing
fi
if [ ! -f output/updating ]; then
	dd skip=64 iflag=skip_bytes if=updating.bmp of=output/updating
fi

convert boot_logo.png -type truecolor output/boot_logo.bmp && gzip -f -n output/boot_logo.bmp

cd output
if [ ! -f data ]; then
	# tar -czvf data installing updating
	zip -r data.zip installing updating
	mv data.zip data
fi

cp ~/buildroot/output/images/rootfs.ext2 ./

cat ../boot.sh > $TARGET
echo BINARY >> $TARGET
uuencode data data >> $TARGET