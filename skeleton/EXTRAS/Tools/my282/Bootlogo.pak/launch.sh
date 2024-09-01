#!/bin/sh

LOGO_NAME="bootlogo.bmp"

overclock.elf performance 2 1200 384 1080 0

DIR="$(dirname "$0")"
cd "$DIR"

LOGO_PATH=$DIR/$LOGO_NAME
if [ ! -f $LOGO_PATH ]; then
	show.elf $DIR/res/missing.png 2
	exit 1
fi

cp /dev/mtdblock0 boot0

VERSION=$(cat /usr/miyoo/version)
OFFSET_PATH="res/offset-$VERSION"

if [ ! -f "$OFFSET_PATH" ]; then
	show.elf $DIR/res/abort.png 2
	exit 1
fi

# offset is found with binwalk mtdblock0
OFFSET=$(cat "$OFFSET_PATH")

show.elf $DIR/res/updating.png 120 &

gzip -k "$LOGO_PATH"
LOGO_PATH=$LOGO_PATH.gz
LOGO_SIZE=$(wc -c < "$LOGO_PATH")

MAX_SIZE=62234
if [ "$LOGO_SIZE" -gt "$MAX_SIZE" ]; then
	show.elf $DIR/res/simplify.png 4
	exit 1
fi

# workaround for missing conv=notrunc support
OFFSET_PART=$((OFFSET+LOGO_SIZE))
dd if=boot0 of=boot0-suffix bs=1 skip=$OFFSET_PART 2>/dev/null
dd if=$LOGO_PATH of=boot0 bs=1 seek=$OFFSET 2>/dev/null
dd if=boot0-suffix of=boot0 bs=1 seek=$OFFSET_PART 2>/dev/null

mtd write "$DIR/boot0" boot
killall -9 show.elf

rm $LOGO_PATH boot0 boot0-suffix
mv $DIR $DIR.disabled
