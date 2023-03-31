#!/bin/sh
# MiniUI.pak

if [ -z "$LCD_INIT" ]; then
	/mnt/SDCARD/.system/miyoomini/bin/blank.elf

	# init backlight
	echo 0 > /sys/class/pwm/pwmchip0/export
	echo 800 > /sys/class/pwm/pwmchip0/pwm0/period
	echo 6 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
	echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

	# init lcd
	cat /proc/ls
	sleep 0.5
fi

# init charger detection
if [ ! -f /sys/devices/gpiochip0/gpio/gpio59/direction ]; then
	echo 59 > /sys/class/gpio/export
	echo in > /sys/devices/gpiochip0/gpio/gpio59/direction
fi

#######################################

export PLATFORM="miyoomini"
export SDCARD_PATH="/mnt/SDCARD"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH=$USERDATA_PATH/.minui/datetime.txt # used by bin/shutdown

mkdir -p "$USERDATA_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$USERDATA_PATH/.minui"

#######################################

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > "$CPU_PATH"

MIYOO_VERSION=`/etc/fw_printenv miyoo_version`
export MIYOO_VERSION=${MIYOO_VERSION#miyoo_version=}

#######################################

# killall tee # NOTE: killing tee is somehow responsible for audioserver crashes
rm -f "$SDCARD_PATH/update.log"

#######################################

export LD_LIBRARY_PATH="/mnt/SDCARD/.system/miyoomini/lib:$LD_LIBRARY_PATH"
export PATH="/mnt/SDCARD/.system/miyoomini/bin:$PATH"

#######################################

# NOTE: could cause performance issues on more demanding cores...maybe?
if [ -f /customer/lib/libpadsp.so ]; then
    LD_PRELOAD=as_preload.so audioserver.mod &
    export LD_PRELOAD=libpadsp.so
fi

#######################################

lumon.elf & # adjust lcd luma and saturation

CHARGING=`cat /sys/devices/gpiochip0/gpio/gpio59/value`
if [ "$CHARGING" == "1" ]; then
	batmon.elf
fi

keymon.elf &

#######################################

# init datetime
if [ -f "$DATETIME_PATH" ]; then
	DATETIME=`cat "$DATETIME_PATH"`
	date +'%F %T' -s "$DATETIME"
	DATETIME=`date +'%s'`
	DATETIME=$((DATETIME + 6 * 60 * 60))
	date -u -s "@$DATETIME"
fi

#######################################

AUTO_PATH=$USERDATA_PATH/auto.sh
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi

cd $(dirname "$0")

#######################################

EXEC_PATH=/tmp/minui_exec
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH"  && sync
while [ -f "$EXEC_PATH" ]; do
	echo ondemand > "$CPU_PATH"
	minui.elf &> $LOGS_PATH/minui.txt
	
	echo `date +'%F %T'` > "$DATETIME_PATH"
	echo performance > "$CPU_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		if [ -f "/tmp/using-swap" ]; then
			swapoff $USERDATA_PATH/swapfile
			rm -f "/tmp/using-swap"
		fi
		
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

shutdown # just in case
