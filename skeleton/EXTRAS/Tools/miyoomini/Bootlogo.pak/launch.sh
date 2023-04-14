#!/bin/sh

cd $(dirname "$0")

{

SUPPORTED_VERSION="202205010000" # there is no 202205010000 firmware, it's when I created this pak originally
if [ $MIYOO_VERSION -gt $SUPPORTED_VERSION ]; then
	echo "Unknown firmware version. Aborted." >> ./log.txt
	exit 1
fi

./logoread.elf

if [ -f ./logo.jpg ]; then
	cp ./logo.jpg ./image1.jpg
else
	cp "$SYSTEM_PATH/dat/image1.jpg" ./
fi

if ! ./logomake.elf; then
	echo "Preparing bootlogo failed. Aborted." >> ./log.txt
	exit 1
fi

if ! ./logowrite.elf; then
	echo "Flashing bootlogo failed. Aborted." >> ./log.txt
	exit 1
fi

echo "Flashed bootlogo successfully. Tidying up."

rm image1.jpg
rm image2.jpg
rm image3.jpg
rm logo.img

echo "Done."

} &> ./log.txt