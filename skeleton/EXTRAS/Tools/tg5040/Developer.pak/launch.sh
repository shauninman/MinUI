#!/bin/sh

cd $(dirname "$0")
{
cat /dev/zero > /dev/fb0

DEVICE=/dev/input/event3
BUTTON="*type 1 (EV_KEY), code 304 (BTN_SOUTH), value 1*"

./evtest "$DEVICE" | while read LINE; do
	case $LINE in
		($BUTTON) killall -9 evtest ;;
	esac
done

cat /dev/zero > /dev/fb0

} &> ./log.txt