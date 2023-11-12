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
if [ ! -f extra-libs.tar ]; then
	find /opt \( -name libpng*.so* -o -name libSDL_image*.so* -o -name libSDL*_ttf*.so* -o -name libSDL-*.so* -o -name libSDL.so \) -print0 | tar --transform 's/.*\///g' -cvf extra-libs.tar --null --files-from -
fi