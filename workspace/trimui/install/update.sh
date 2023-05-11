#!/bin/sh

# NOTE: becomes .system/trimui/bin/install.h

INSTALL_PATH=$SDCARD_PATH/trimui.zip
if [ -f "$INSTALL_PATH" ]; then
	rm -f "$INSTALL_PATH"
fi
