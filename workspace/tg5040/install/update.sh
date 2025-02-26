#!/bin/sh

SDCARD_PATH=/mnt/SDCARD

# --------------------------------------
# remove old brick system folder
BRICK_PATH=${SDCARD_PATH}/.system/tg3040
echo "check for $BRICK_PATH"
# this might always exist so we can pull up old cards
if [ -d $BRICK_PATH ]; then
	echo "deleting brick system folder $BRICK_PATH"
	rm -rf "$BRICK_PATH"
	
	# copy brick configs from userdata
	SRC_PATH=${SDCARD_PATH}/.userdata/tg3040
	if [ -d $SRC_PATH ]; then
		DST_PATH=${SDCARD_PATH}/.userdata/tg5040
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
						echo "copying $CFG_PATH to $NEW_PATH/$CFG_NAME-brick.cfg"
						cp $CFG_PATH $NEW_PATH/$CFG_NAME-brick.cfg
					fi
				done
			fi
		done
		echo "deleting brick userdata $SRC_PATH"
		rm -rf $SRC_PATH
		
		UPDATE_PATH=${SDCARD_PATH}/.tmp_update/tg3040
		rm -rf $UPDATE_PATH.sh
		rm -rf $UPDATE_PATH
		
		reboot
		# we need to sleep until reboot otherwise 
		# it will poweroff without rebooting
		while :; do
			sleep 1
		done
	fi
fi
# --------------------------------------
