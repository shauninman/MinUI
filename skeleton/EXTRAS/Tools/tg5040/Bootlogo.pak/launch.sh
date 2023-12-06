#!/bin/sh

set -ex

DIR=$(dirname "$0")
cd $DIR

# 396x66 (or fewer pixels)
# Save as BMP
# Windows
# 32-bit
# DO NOT flip row order

{

LOGO_PATH=$DIR/bootlogo.bmp
OFFSET=42496
SIZE=104600

if [ ! -f $LOGO_PATH ]; then
	echo "No logo.bmp available. Aborted."
	exit 1
fi

COUNT=$(du -k ./bootlogo.bmp | cut -f1)
if [ $COUNT -gt $SIZE ]; then
	echo "logo.bmp too large ($COUNT). Aborted."
	exit 1
fi

dd if=/dev/mmcblk0p1 of=/tmp/bootlogo.sig bs=1 skip=$OFFSET count=2
SIGNATURE=`cat /tmp/bootlogo.sig`
if [ "$SIGNATURE" = "BM" ]; then
	echo "Found bitmap at offset $OFFSET"
else
	echo "Bad offset. Aborted."
	exit 1
fi

# inject new
echo "injecting $LOGO_PATH"
dd conv=notrunc if=$LOGO_PATH of=/dev/mmcblk0p1 bs=1 seek=$OFFSET

sync
echo "Done."

} &> ./log.txt

# self-destruct
mv $DIR $DIR.disabled

reboot