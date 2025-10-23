#!/bin/sh
cd $(dirname "$0")

IP=$(ip -4 addr show dev wlan0 | awk '/inet / {print $2}' | cut -d/ -f1)
if [ -z "$IP" ]; then
	IP=Unassigned
fi

say.elf $IP

