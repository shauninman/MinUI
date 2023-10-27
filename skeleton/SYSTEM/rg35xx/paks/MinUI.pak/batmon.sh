#!/bin/sh

touch /mnt/sdcard/batmon.txt
while :; do
	O=`cat /sys/class/power_supply/battery/charger_online`
	C=`cat /sys/class/power_supply/battery/capacity`
	V=`cat /sys/class/power_supply/battery/voltage_now`
	
	# should match PWR_updateBatteryStatus() in api.c
	M=$(($V/10000))
	M=$(($M-310))
	
	if   [ $M -gt 80 ]; then M=100;
	elif [ $M -gt 60 ]; then M=80;
	elif [ $M -gt 40 ]; then M=60;
	elif [ $M -gt 20 ]; then M=40;
	elif [ $M -gt 10 ]; then M=20;
	else 					 M=10; fi
	
	N=`date`
	echo "$C ($O:$M) $V $N" >> /mnt/sdcard/batmon.txt
	sync
	sleep 5
done