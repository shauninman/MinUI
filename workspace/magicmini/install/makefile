SHELL=/bin/bash
DTB=rk3562-magicx-linux

all:
	dtc -I dts -O dtb -o $(DTB).dtb $(DTB).dts 2> >(grep -v "Warning")