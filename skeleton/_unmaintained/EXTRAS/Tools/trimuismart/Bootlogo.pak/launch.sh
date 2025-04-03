#!/bin/sh

cd $(dirname "$0")

{

LOGO_PATH=./logo.bmp

if [ ! -f $LOGO_PATH ]; then
	LOGO_PATH=$SYSTEM_PATH/dat/logo.bmp
fi

if [ ! -f $LOGO_PATH ]; then
	echo "No logo.bmp available. Aborted"
	exit 1
fi

dd if=$LOGO_PATH of=/dev/by-name/bootlogo bs=65536

echo "Done."

} &> ./log.txt

# self-destruct
DIR=$(dirname "$0")
mv $DIR $DIR.disabled
