#!/bin/sh

IS_PLUS=false
if [ -f "/customer/app/axp_test" ]; then
	IS_PLUS=true
fi

if $IS_PLUS; then
	MIYOO_DIR="miyoo354"
else
	MIYOO_DIR="miyoo"
fi


SDCARD_PATH=/mnt/SDCARD
cd "$SDCARD_PATH/$MIYOO_DIR/app"

export LD_LIBRARY_PATH=/lib:/config/lib:/customer/lib

# update bootcmd if necessary
contains() { [ -z "${2##*$1*}" ]; }
MIYOO_VERSION=`/etc/fw_printenv miyoo_version`
MIYOO_VERSION=${MIYOO_VERSION#miyoo_version=}
SUPPORTED_VERSION="202205010000" # date after latest known version
# TODO: pretty sure this bricks a subsequent update
if [ $MIYOO_VERSION -lt $SUPPORTED_VERSION ]; then
	OLD_CMD=`/etc/fw_printenv bootcmd`
	NEW_CMD="gpio output 85 1; bootlogo 0 0 0 0 0; mw 1f001cc0 11; gpio out 8 0; sf probe 0;sf read 0x22000000 \${sf_kernel_start} \${sf_kernel_size}; gpio out 8 1; gpio output 4 1; bootm 0x22000000"
	if contains "sleepms" "$OLD_CMD"; then
		/etc/fw_setenv bootcmd $NEW_CMD
		sleep 1
	fi
fi

# .tmp_update/updater does the actual installation (and later, updating)
cp -rf .tmp_update $SDCARD_PATH/
rm -rf "$SDCARD_PATH/$MIYOO_DIR"
sync
$SDCARD_PATH/.tmp_update/updater

# under no circumstances should stock be allowed to touch this card
if $IS_PLUS; then
	poweroff
else
	reboot
fi