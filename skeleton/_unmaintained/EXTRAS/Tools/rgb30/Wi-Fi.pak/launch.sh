#!/bin/sh

cd "$(dirname "$0")"

OLDIFS=$IFS
IFS=$'\n' read -a WIFI -d '' < ./wifi.txt
IFS=$OLDIFS

WIFI_NAME=${WIFI[0]}
WIFI_PASS=${WIFI[1]}

##############

. /etc/profile # NOTE: this nukes NextUI's PATH modifications
PATH=/storage/roms/.system/rgb30/bin:$PATH

CUR_NAME=`get_setting wifi.ssid`
CUR_PASS=`get_setting wifi.key`

RES_PATH="$(dirname "$0")/res"
STATUS=$(cat "/sys/class/net/wlan0/operstate")

function disconnect()
{
	echo "disconnect"
	
	wifictl disable &
	show.elf "$RES_PATH/disable.png" 2
	STATUS=down
}

function connect()
{
	echo "connect"
	wifictl enable &
	DELAY=30
	show.elf "$RES_PATH/enable.png" $DELAY &
	for i in `seq 1 $DELAY`; do
		STATUS=$(cat "/sys/class/net/wlan0/operstate")
		if [ "$STATUS" = "up" ]; then
			break
		fi
		sleep 1
	done
	killall -s KILL show.elf
}

{
if [ "$WIFI_NAME" != "$CUR_NAME" ] || [ "$WIFI_PASS" != "$CUR_PASS" ]; then
	echo "change"
	
	if [ "$STATUS" = "up" ]; then
		disconnect
	fi
	
	echo "$WIFI_NAME:$WIFI_PASS"
	show.elf "$RES_PATH/update.png" 2
	set_setting wifi.ssid "$WIFI_NAME"
	set_setting wifi.key "$WIFI_PASS"
fi

echo "toggle"
if [ "$STATUS" = "up" ]; then
	disconnect
else
	connect
fi
} &> ./log.txt