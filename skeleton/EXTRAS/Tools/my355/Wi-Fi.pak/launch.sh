#!/bin/sh
cd $(dirname "$0")

# if wpa_supplicant is already running, disable it
if [ -n "$(pidof wpa_supplicant)" ]; then
    show.elf res/disable.png 3 &
    killall wpa_supplicant
    sleep 3s
    killall show.elf
    exit
fi

# otherwise, load the kernel module and start wpa_supplicant
show.elf res/enable.png 6 &

echo 1 > /sys/class/rfkill/rfkill1/state
insmod /system/lib/modules/RTL8188FU.ko

/usr/sbin/wpa_supplicant -B -D nl80211 -iwlan0 -c /userdata/cfg/wpa_supplicant.conf &

sleep 6s
killall show.elf
