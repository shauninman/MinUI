#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
if [ "$DEVICE" = "brick" ]; then
    CFG="tg3040.cfg"
else
    CFG="tg5040.cfg"
fi

./NextCommander --config $CFG