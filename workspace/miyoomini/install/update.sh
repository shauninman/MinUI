#!/bin/sh

# NOTE: becomes .system/miyoomini/bin.install.h

# TODO: alpha only
SDCARD_PATH="/mnt/SDCARD"
ARCH_PATH="$SDCARD_PATH/.userdata/arm-480"
SHARED_PATH="$SDCARD_PATH/.userdata/shared"
if [ -d "$ARCH_PATH" ] && [ ! -d "$SHARED_PATH" ]; then
	mv "$ARCH_PATH" "$SHARED_PATH"
fi
