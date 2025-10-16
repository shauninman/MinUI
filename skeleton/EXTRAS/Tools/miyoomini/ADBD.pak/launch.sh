#!/bin/sh
DIR=$(dirname "$0")
cd $DIR

# bail if already running
if pidof adbd >/dev/null 2>&1; then
	exit 0
fi

# load the wifi driver...from the SD card :lolsob:
insmod ./8188fu.ko

# superstitious ritual? nope!
/customer/app/axp_test wifion
sleep 2

# bring up wifi (configured in stock)
ifconfig wlan0 up
wpa_supplicant -B -D nl80211 -iwlan0 -c /appconfigs/wpa_supplicant.conf
udhcpc -i wlan0 -s /etc/init.d/udhcpc.script &

# surgical strike to nop /etc/profile
# because it brings up the entire system again
mount -o bind $DIR/profile /etc/profile

# actually launch adbd
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
./adbd &

