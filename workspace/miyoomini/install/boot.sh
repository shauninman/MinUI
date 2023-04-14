#!/bin/sh

# NOTE: becomes .tmp_update/updater

# TODO: this file is shared with many Trimui and Miyoo devices, which owns it?
# TODO: add a "many" folder to workspace for stuff like this?
# TODO: copy boot folder to build/BASE/ as .tmp_update and rename boot.sh to updater

# TODO: Linux version dffers 
# 	smart	3.4.39
#	mini	4.9.84 
#	models	3.10.65

# VERSION=`cat /proc/version 2> /dev/null`
# case $VERSION in
# *"@alfchen-ubuntu"*)
# 	SYSTEM_NAME="trimui"
# 	;;
# *"@alfchen-NUC"*)
# 	SYSTEM_NAME="miyoomini" # or smart :thinking_face:
# 	;;
# esac

PLATFORM="miyoomini"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

# TODO: this might not be available on trimuis, check!
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > "$CPU_PATH"

# install/update
if [ -f "$UPDATE_PATH" ]; then 
	cd $(dirname "$0")
	if [ -d "$SYSTEM_PATH" ]; then
		# init backlight
		echo 0 > /sys/class/pwm/pwmchip0/export
		echo 800 > /sys/class/pwm/pwmchip0/pwm0/period
		echo 50 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
		echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

		# init lcd
		cat /proc/ls
		sleep 0.5
		export LCD_INIT=1

		./show.elf ./updating.png
	else
		./show.elf ./installing.png
	fi
	
	unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH"
	rm -f "$UPDATE_PATH"
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/$PLATFORM/bin/install.sh # &> $SDCARD_PATH/install.txt
fi

# or launch (and keep launched)
LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

reboot # under no circumstances should stock be allowed to touch this card