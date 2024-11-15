#!/bin/bash

cd "$(dirname "$0")"

OLDIFS=$IFS
IFS=$'\n' read -a WIFI -d '' < ./wifi.txt
IFS=$OLDIFS

WIFI_NAME=${WIFI[0]}
WIFI_PASS=${WIFI[1]}

##############

RES_PATH="$(dirname "$0")/res"

toggle_wifi() {
    wifi_status=$(nmcli radio wifi)
		
    if [ "$wifi_status" = "disabled" ]; then
	    echo "Wi-Fi is currently OFF. Turning it ON and connecting to $WIFI_NAME..."

        nmcli radio wifi on
        nmcli device wifi connect "$WIFI_NAME" password "$WIFI_PASS" &

		DELAY=30
		show.elf "$RES_PATH/enable.png" $DELAY &
		for i in `seq 1 $DELAY`; do
			STATUS=$(cat "/sys/class/net/wlan0/operstate")
			if [ "$STATUS" = "up" ]; then
				break
			fi
			sleep 1
		done

    else ### if [ "$wifi_status" != "disabled" ]; then

        current_ssid=$(nmcli -t -f ACTIVE,SSID device wifi | grep '^*' | cut -d: -f2)

        if [ "$current_ssid" = "$WIFI_NAME" ]; then
            echo "Already connected to $WIFI_NAME. Disconnecting and turning off Wi-Fi..."

			show.elf "$RES_PATH/disable.png" 2
			
            nmcli device disconnect wlan0
            nmcli radio wifi off
        else ### [ "$current_ssid" != "$WIFI_NAME" ]; then
            echo "Wi-Fi is ON but not connected to $WIFI_NAME. Connecting now..."

			show.elf "$RES_PATH/update.png" 2

            nmcli device wifi connect "$WIFI_NAME" password "$WIFI_PASS"
        fi
    fi
}

toggle_wifi > ./log.txt 2>&1