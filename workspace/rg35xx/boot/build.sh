#!/bin/sh

TARGET=dmenu.bin

mkdir -p output
if [ ! -f output/installing ]; then
	dd skip=64 iflag=skip_bytes if=installing.bmp of=output/installing
fi
if [ ! -f output/updating ]; then
	dd skip=64 iflag=skip_bytes if=updating.bmp of=output/updating
fi

cd output
if [ ! -f data ]; then
	# tar -czvf data installing updating
	zip -r data.zip installing updating
	mv data.zip data
fi

cat ../boot.sh > $TARGET
echo BINARY >> $TARGET
uuencode data data >> $TARGET
