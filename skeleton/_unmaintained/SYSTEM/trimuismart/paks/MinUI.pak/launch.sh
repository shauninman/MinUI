#!/bin/sh
# MiniUI.pak

# recover from readonly SD card -------------------------------
touch /mnt/writetest
sync
if [ -f /mnt/writetest ] ; then
	rm -f /mnt/writetest
else
	e2fsck -p /dev/root > /mnt/SDCARD/RootRecovery.txt
	reboot
fi

#######################################

export PLATFORM="trimuismart"
export SDCARD_PATH="/mnt/SDCARD"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

mkdir -p "$USERDATA_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"

#######################################

echo A,B,X,Y,L,R > /sys/module/gpio_keys_polled/parameters/button_config

echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=1536000
echo $CPU_SPEED_PERF > $CPU_PATH

#######################################

export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:/usr/trimui/lib:$LD_LIBRARY_PATH
export PATH=$SYSTEM_PATH/bin:/usr/trimui/bin:$PATH

leds_off

killall -9 MtpDaemon
killall -9 wpa_supplicant
ifconfig wlan0 down

keymon.elf &

#######################################

# init datetime
if [ -f "$DATETIME_PATH" ]; then
	DATETIME=`cat "$DATETIME_PATH"`
	date +'%F %T' -s "$DATETIME"
	DATETIME=`date +'%s'`
	date -u -s "@$DATETIME"
fi

#######################################

AUTO_PATH=$USERDATA_PATH/auto.sh
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi

cd $(dirname "$0")

#######################################

EXEC_PATH="/tmp/nextui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH"  && sync
while [ -f $EXEC_PATH ]; do
	nextui.elf &> $LOGS_PATH/nextui.txt
	echo $CPU_SPEED_PERF > $CPU_PATH
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		echo $CPU_SPEED_PERF > $CPU_PATH
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
	
	# physical powerswitch, enter low power mode
	if [ -f "/tmp/poweroff" ]; then
		rm -f "/tmp/poweroff"
		killall keymon.elf
		shutdown
		echo 60000 > $CPU_PATH
		LED_ON=true
		while :; do
			if $LED_ON; then
				LED_ON=false
				leds_off
			else
				LED_ON=true
				leds_on
			fi
			sleep 0.5
		done
	fi
done

poweroff # just in case
