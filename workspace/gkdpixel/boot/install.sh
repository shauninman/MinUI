#!/bin/sh

cd $(dirname "$0")

if [ ! -f /usr/sbin/frontend_start.original ]; then
	mv /usr/sbin/frontend_start /usr/sbin/frontend_start.original
fi

cp boot.sh /usr/sbin/frontend_start

mkdir -p /usr/share/minui
cp *.bmp /usr/share/minui

/usr/sbin/frontend_start