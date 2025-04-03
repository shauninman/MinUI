#!/bin/sh

SYSTEM_PATH=/sdcard/.system/m17
tar xf $SYSTEM_PATH/dat/extra-libs.tar -C $SYSTEM_PATH/lib

cp $SYSTEM_PATH/dat/em_ui.sh /sdcard
sync