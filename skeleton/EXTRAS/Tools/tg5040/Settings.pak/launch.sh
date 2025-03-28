#!/bin/sh

SDCARD_PATH="/mnt/SDCARD"
USERDATA_PATH="$SDCARD_PATH/.userdata"
SHARED_USERDATA_PATH="$USERDATA_PATH/shared"

cd $(dirname "$0")
./settings.elf > settings.log 2>&1
