#!/bin/sh

export PLATFORM="rg35xxplus"
export SDCARD_PATH="/mnt/sdcard"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"
export HDMI_EXPORT_PATH="/tmp/hdmi_export.sh"

#######################################

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

#######################################

systemctl disable ondemand
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo 0 > /sys/class/power_supply/axp2202-battery/work_led

export RGXX_MODEL=`strings /mnt/vendor/bin/dmenu.bin | grep ^RG`
# export RGXX_TIMESTAMP=`strings /mnt/vendor/bin/dmenu.bin | grep ^202`
# export RGXX_VERSION=`strings /mnt/vendor/bin/dmenu.bin | grep -P ^V[0-9]+`

if [ "$RGXX_MODEL" = "RGcubexx" ]; then
	export DEVICE="cube"
elif [ "$RGXX_MODEL" = "RG34xx" ]; then
	export DEVICE="wide"
fi

#######################################

keymon.elf & # > $LOGS_PATH/keymon.txt 2>&1 &

#######################################

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"
AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # > $LOGS_PATH/auto.txt 2>&1
fi

cd $(dirname "$0")

#######################################

init.elf # > $LOGS_PATH/init.txt 2>&1

#######################################

hdmimon.sh & # > $LOGS_PATH/hdmimon.txt 2>&1 &

HDMI_STATE_PATH="/sys/class/switch/hdmi/cable.0/state"
HAS_HDMI=$(cat "$HDMI_STATE_PATH")
if [ "$HAS_HDMI" = "1" ]; then
	sleep 3
fi

#######################################

EXEC_PATH="/tmp/nextui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	. $HDMI_EXPORT_PATH
	nextui.elf > $LOGS_PATH/nextui.txt 2>&1
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		. $HDMI_EXPORT_PATH
		. $NEXT_PATH
		rm -f $NEXT_PATH
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

shutdown
