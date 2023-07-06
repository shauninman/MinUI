#!/bin/sh

export PLATFORM="rg353"
export SDCARD_PATH="/mnt/SDCARD"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"

#######################################

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

export SDL_VIDEODRIVER=fbcon
export SDL_AUDIODRIVER=alsa
export SDL_NOMOUSE=1
export SDL_DEBUG=1 # TODO: tmp

amixer cset name='Playback Path' 'SPK'

# TODO: this works!
# mount -o remount,rw /mnt/BOOT

#######################################

# export CPU_SPEED_MENU=504000
# export CPU_SPEED_GAME=1296000
# export CPU_SPEED_PERF=1488000
# echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# overclock.elf $CPU_SPEED_PERF

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

EXEC_PATH=/tmp/minui_exec
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	# overclock.elf $CPU_SPEED_PERF
	minui.elf &> $LOGS_PATH/minui.txt
	sync
	
	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		# overclock.elf $CPU_SPEED_PERF
		sync
	fi
done