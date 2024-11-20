#!/bin/sh

cd $(dirname "$0")

HDMI_WIDTH=1280
HDMI_HEIGHT=720

DEVICE_WIDTH=640
DEVICE_HEIGHT=480
if [ "$RGXX_MODEL" = "RGcubexx" ]; then
	DEVICE_WIDTH=720
	DEVICE_HEIGHT=720
elif [ "$RGXX_MODEL" = "RG28xx" ]; then # rotated
	TMP_WIDTH=$DEVICE_WIDTH
	DEVICE_WIDTH=$DEVICE_HEIGHT
	DEVICE_HEIGHT=$TMP_WIDTH
fi

DISP_PATH="/sys/kernel/debug/dispdbg"
BLANK_PATH="/sys/class/graphics/fb0/blank"
touch $HDMI_EXPORT_PATH

ALSA_CONF_DIR=/usr/share/alsa/alsa.conf.d
mkdir -p $ALSA_CONF_DIR

enable_hdmi() {
	echo 'enabling HDMI'
	
	echo 4 > $BLANK_PATH
	cat /dev/zero > /dev/fb0
	
	echo disp0 > $DISP_PATH/name
	echo switch > $DISP_PATH/command
	echo "4 10 0 0 0x4 0x101 0 0 0 8" > $DISP_PATH/param
	echo 1 > $DISP_PATH/start
	
	fbset -fb /dev/fb0 -g 0 0 0 0 32
	sleep 0.25
	fbset -fb /dev/fb0 -g $HDMI_WIDTH $HDMI_HEIGHT $HDMI_WIDTH $((HDMI_HEIGHT * 2)) 32
	sleep 0.25
	echo 0 > $BLANK_PATH
	
	echo "export AUDIODEV='hw:2,0'" > $HDMI_EXPORT_PATH
}

disable_hdmi() {
	echo 'disabling HDMI'
	
	echo 4 > $BLANK_PATH
	cat /dev/zero > /dev/fb0
	
	echo disp0 > $DISP_PATH/name
	echo switch > $DISP_PATH/command
	echo "1 0" > $DISP_PATH/param
	echo 1 > $DISP_PATH/start
	
	fbset -fb /dev/fb0 -g 0 0 0 0 32
	sleep 0.25
	fbset -fb /dev/fb0 -g $DEVICE_WIDTH $DEVICE_HEIGHT $DEVICE_WIDTH $((DEVICE_HEIGHT * 2)) 32
	sleep 0.25
	echo 0 > $BLANK_PATH
	
	echo "unset AUDIODEV" > $HDMI_EXPORT_PATH
}

HDMI_STATE_PATH="/sys/class/switch/hdmi/cable.0/state"
HAD_HDMI=$(cat "$HDMI_STATE_PATH")
if [ "$HAD_HDMI" = "1" ]; then
	enable_hdmi
fi

while :; do
	HAS_HDMI=$(cat "$HDMI_STATE_PATH")
	if [ "$HAS_HDMI" != "$HAD_HDMI" ]; then
		if [ "$HAS_HDMI" = "1" ]; then
			enable_hdmi
		else
			disable_hdmi
		fi
	fi
	HAD_HDMI=$HAS_HDMI
	sleep 2
done