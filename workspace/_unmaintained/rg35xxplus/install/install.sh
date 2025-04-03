#!/bin/sh

TF1_PATH=/mnt/mmc
TF2_PATH=/mnt/sdcard # TF1 should be linked to this path if TF2 is missing or doesn't contain our system folder
SYSTEM_PATH=${TF2_PATH}/.system/rg35xxplus

# make sure dmenu stays up to date
echo "update dmenu.bin"
cp $SYSTEM_PATH/dat/dmenu.bin $TF1_PATH
sync

# --------------------------------------
# remove old cube system folder
CUBE_PATH=${TF2_PATH}/.system/rg40xxcube
echo "check for $CUBE_PATH"
# this will always exist so we can pull up old cards
if [ -d $CUBE_PATH ]; then
	echo "deleting cube system folder $CUBE_PATH"
	rm -rf "$CUBE_PATH"
	
	# copy cube configs from userdata
	SRC_PATH=${TF2_PATH}/.userdata/rg40xxcube
	if [ -d $SRC_PATH ]; then
		DST_PATH=${TF2_PATH}/.userdata/rg35xxplus
		mkdir -p $DST_PATH # just in case
	
		for SUB_PATH in $SRC_PATH/*; do
			if [ -d $SUB_PATH ]; then
				SUB_NAME=$(basename $SUB_PATH)
				NEW_PATH=$DST_PATH/$SUB_NAME
			
				if [ ! -d $NEW_PATH ]; then
					echo "creating new path $NEW_PATH"
					mkdir -p $NEW_PATH
				fi
			
				for CFG_PATH in $SUB_PATH/*.cfg; do
					if [ -f $CFG_PATH ]; then
						CFG_NAME=$(basename $CFG_PATH .cfg)
						echo "copying $CFG_PATH to $NEW_PATH/$CFG_NAME-cube.cfg"
						cp $CFG_PATH $NEW_PATH/$CFG_NAME-cube.cfg
					fi
				done
			fi
		done
		echo "deleting cube userdata $SRC_PATH"
		rm -rf $SRC_PATH
		reboot
	fi
fi
# --------------------------------------
