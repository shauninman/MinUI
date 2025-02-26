#!/bin/sh
LCDAEMON_PATH="/etc/LedControl"

cd $(dirname "$0")
chmod a+w /sys/class/led_anim/* >> launch.log

mkdir -p $LCDAEMON_PATH >> launch.log
cp ./lcservice /etc/init.d/ >> launch.log
cp ./lcdaemon $LCDAEMON_PATH >> launch.log
# Check if the file does not exist in the target path
# Copy the file and append output to launch.log
TARGET_PATH="$LCDAEMON_PATH/settings.txt"
if [ ! -f "$TARGET_PATH" ]; then
    cp ./settings.txt "$LCDAEMON_PATH" >> launch.log
    echo "File copied to $LCDAEMON_PATH" >> launch.log
else
    echo "File already exists in $LCDAEMON_PATH" >> launch.log
fi
chmod +x /etc/init.d/lcservice >> launch.log
/etc/init.d/lcservice enable >> launch.log


# Check if deamon is running
if pgrep -f "lcdaemon" >/dev/null; then
    echo "lcdaemon is already running" >> launch.log
    killall lcdaemon
fi

/etc/LedControl/lcdaemon > lcdaemon.log 2>&1 &
echo "lcdaemon started" >> launch.log


./main > ledcontrol.log 2>&1
