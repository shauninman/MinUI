#!/bin/sh

TF1_PATH=/mnt/mmc
TF2_PATH=/mnt/sdcard # TF1 should be linked to this path if TF2 is missing or doesn't contain our system folder
SYSTEM_PATH=${TF2_PATH}/.system/rg35xxplus

# make sure dmenu stays up to date
cp $SYSTEM_PATH/dat/dmenu.bin $TF1_PATH