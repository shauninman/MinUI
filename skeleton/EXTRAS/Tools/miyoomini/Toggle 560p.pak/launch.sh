#!/bin/sh

FILE=$USERDATA_PATH/enable-560p
if [ -f $FILE ]; then
	rm $FILE
else
	touch $FILE
fi

