#!/bin/sh

TARGET=dmenu.bin

# TODO: copy boot.sh from rg35xx and rg35xxplus

tar -cvf data boot-rg35xx*

cat boot.sh > $TARGET
echo PAYLOAD >> $TARGET
cat data data >> $TARGET
echo >> $TARGET