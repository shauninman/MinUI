#!/bin/sh

export PLATFORM="rgb30"
export SDCARD_PATH="/storage/roms"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

#######################################

# JELOS aggressively repairs disks and leaves litter on the SD card
rm $SDCARD_PATH/FSCK*.REC

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

# required for sdl12-compat
export SDL_VIDEODRIVER=kmsdrm
export SDL_AUDIODRIVER=alsa

# required for sdl1 fbcon
# export SDL_VIDEODRIVER=fbcon
# export SDL_AUDIODRIVER=alsa
# export SDL_NOMOUSE=1

echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000
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
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		echo $CPU_SPEED_PERF > $CPU_PATH
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

shutdown # just in case
