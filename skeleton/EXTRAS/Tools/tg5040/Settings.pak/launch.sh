#!/bin/sh
SETTINGS_PATH="/mnt/SDCARD/.userdata"
TARGET_PATH="$SETTINGS_PATH/minuisettings.txt"

cd $(dirname "$0")
if [ ! -f "$TARGET_PATH" ]; then
    cp ./minuisettings.txt "$SETTINGS_PATH" >> launch.log
    echo "File copied to $SETTINGS_PATH" >> launch.log
else
    echo "File already exists in $SETTINGS_PATH" >> launch.log
fi

./main > settings.log 2>&1
