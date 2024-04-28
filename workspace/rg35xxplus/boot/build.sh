#!/bin/sh

SOURCE=boot.sh
TARGET=dmenu.bin

# TODO: test
# SOURCE=test.sh
# TARGET=launch.sh

mkdir -p output
if [ ! -f output/installing ]; then
	dd skip=54 iflag=skip_bytes if=installing.bmp of=output/installing
fi
if [ ! -f output/updating ]; then
	dd skip=54 iflag=skip_bytes if=updating.bmp of=output/updating
fi
if [ ! -f output/bootlogo.bmp ]; then
	cp bootlogo.bmp ./output/
fi

# rotated for 28xx
if [ ! -f output/installing-r ]; then
	dd skip=54 iflag=skip_bytes if=installing-r.bmp of=output/installing-r
fi
if [ ! -f output/updating-r ]; then
	dd skip=54 iflag=skip_bytes if=updating-r.bmp of=output/updating-r
fi
if [ ! -f output/bootlogo.bmp-r ]; then
	cp bootlogo-r.bmp ./output/
fi

cp ../other/unzip60/unzip ./output/

cd output

# TODO: copy unzip from other/unzip60

tar -czvf data bootlogo.bmp installing updating bootlogo-r.bmp installing-r updating-r unzip

cat ../$SOURCE > $TARGET
echo BINARY >> $TARGET
cat data >> $TARGET
echo >> $TARGET
