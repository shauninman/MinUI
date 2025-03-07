#!/bin/sh
cd $(dirname "$0")
chmod a+w /sys/class/led_anim/* >> launch.log


TARGET_PATH="/mnt/SDCARD/.userdata/shared/ledsettings.txt"
if [ ! -f "$TARGET_PATH" ]; then
    cp ./settings.txt /mnt/SDCARD/.userdata/shared/ledsettings.txt >> launch.log
    echo "File copied to $LCDAEMON_PATH" >> launch.log
else
    echo "File already exists in TARGET_PATH" >> launch.log
fi


./ledcontrol.elf > ledcontrol.log 2>&1
