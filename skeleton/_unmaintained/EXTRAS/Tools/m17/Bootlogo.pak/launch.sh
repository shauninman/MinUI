#!/bin/sh

DIR=$(dirname "$0")
cd $DIR

# 264x40 (or fewer pixels)
# Save as BMP
# Windows
# 24-bit
# DO NOT flip row order

{

LOGO_PATH=$DIR/logo.bmp

if [ ! -f $LOGO_PATH ]; then
	echo "No logo.bmp available. Aborted."
	exit 1
fi

# read new bitmap size
HEX=`dd if=$LOGO_PATH bs=1 skip=2 count=4 status=none | xxd -g4 -p`
# convert to decimal (need to swap le bytes)
COUNT=$((0x${HEX:6:2}${HEX:4:2}${HEX:2:2}${HEX:0:2}))
if [ $COUNT -gt 32768 ]; then
	echo "logo.bmp too large ($COUNT). Aborted."
	exit 1
fi

OFFSET=4044800 # rev A
SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`

if [ "$SIGNATURE" = "BM" ]; then
	echo "Rev A"
else
	OFFSET=4045312 # rev B
	SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`
	if [ "$SIGNATURE" = "BM" ]; then
		echo "Rev B"
	else
		OFFSET=4046848 # rev C
		SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`
		if [ "$SIGNATURE" = "BM" ]; then
			echo "Rev C"
		else
			echo "Rev unknown. Aborted."
			exit 1
		fi
	fi
fi

# current timestamp
DT=`date +'%Y%m%d%H%M%S'`
# read bitmap size
HEX=`dd if=/dev/block/by-name/boot bs=1 skip=$(($OFFSET+2)) count=4 status=none | xxd -g4 -p`
# convert to decimal (need to swap le bytes)
COUNT=$((0x${HEX:6:2}${HEX:4:2}${HEX:2:2}${HEX:0:2}))
# create backup
echo "copying $COUNT bytes from $OFFSET to backup-$DT.bmp"
dd if=/dev/block/by-name/boot of=./backup-$DT.bmp bs=1 skip=$OFFSET count=$COUNT

# inject new
echo "injecting $LOGO_PATH"
dd conv=notrunc if=$LOGO_PATH of=/dev/block/by-name/boot bs=1 seek=$OFFSET

sync
echo "Done."

} &> ./log.txt

# self-destruct
mv $DIR $DIR.disabled