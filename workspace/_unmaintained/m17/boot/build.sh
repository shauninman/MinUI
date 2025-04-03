#!/bin/sh

# based on rg35xx implementation

TARGET=em_ui.sh

# remove header from bitmaps
mkdir -p output
if [ ! -f output/installing ]; then
	dd bs=54 skip=1 if=installing.bmp of=output/installing
fi
if [ ! -f output/updating ]; then
	dd bs=54 skip=1 if=updating.bmp of=output/updating
fi

cd output

# zip headerless bitmaps
if [ ! -f data ]; then
	zip -r data.zip installing updating
	mv data.zip data
fi

# encode and add to end of boot.sh
cat ../boot.sh > $TARGET
echo BINARY >> $TARGET
uuencode data data >> $TARGET

# tar missing libs from toolchain
if [ ! -f libpng16.so.16 ] || [ ! -f libSDL2_ttf-2.0.so.0 ]; then
	cp /opt/m17-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/libpng16.so.16.34.0 libpng16.so.16
	cp /opt/m17-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/libSDL2_ttf-2.0.so.0.14.0 libSDL2_ttf-2.0.so.0
fi