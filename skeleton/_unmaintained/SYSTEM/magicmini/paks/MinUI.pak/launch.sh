#!/bin/sh

# dmesg > /storage/TF1/dmesg.txt

export PLATFORM="magicmini"
export SDCARD_PATH="/storage/TF2"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

export SDL_AUDIODRIVER=alsa
amixer cset name='Playback Path' SPK # or HP, seems to switch automatically

cat /dev/zero > /dev/fb0

export CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
export CPU_SPEED_PERF=1608000
echo performance > /sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor
echo performance > /sys/devices/platform/dmc/devfreq/dmc/governor
echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo $CPU_SPEED_PERF > $CPU_PATH

#######################################

keymon.elf & # &> $SDCARD_PATH/keymon.txt &

#######################################

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"
AUTO_PATH=$USERDATA_PATH/auto.sh
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # &> $LOGS_PATH/auto.txt
fi

cd $(dirname "$0")

#######################################

EXEC_PATH=/tmp/nextui_exec
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	echo $CPU_SPEED_PERF > $CPU_PATH
	nextui.elf &> $LOGS_PATH/nextui.txt
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		echo $CPU_SPEED_PERF > $CPU_PATH
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done
