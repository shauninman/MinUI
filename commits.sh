#!/bin/bash

show() {
	pushd "$1" >> /dev/null
	HASH=$(git rev-parse --short=8 HEAD)
	NAME=$(basename $PWD)
	REPO=$(git config --get remote.origin.url)
	REPO=$(sed -E "s,(^git@github.com:)|(^https?://github.com/)|(.git$)|(/$),,g" <<<"$REPO")
	popd >> /dev/null

	printf '%-24s%-10s%s\n' $NAME $HASH $REPO
}
list() {
	pushd "$1" >> /dev/null
	for D in ./*; do
		show "$D"
	done
	popd >> /dev/null
}
tell() {
	printf '%s\n--------\n' $1
}
bump() {
	printf '\n'
}

{
	tell MINUI
	show ./
	bump
	
	tell TOOLCHAINS
	list ./toolchains
	bump
	
	tell LIBRETRO
	show ./workspace/all/minarch/libretro-common
	bump
	
	tell RG35XX
	show ./workspace/rg35xx/other/DinguxCommander
	echo CORES
	list ./workspace/rg35xx/cores/src
	bump
	
	tell MIYOOMINI
	show ./workspace/miyoomini/other/DinguxCommander
	show ./workspace/miyoomini/other/sdl
	echo CORES
	list ./workspace/miyoomini/cores/src
	bump
	
	tell TRIMUISMART
	show ./workspace/trimuismart/other/DinguxCommander
	show ./workspace/trimuismart/other/unzip60
	echo CORES
	list ./workspace/trimuismart/cores/src
	bump
	
	tell RGB30
	show ./workspace/rgb30/other/DinguxCommander
	echo CORES
	list ./workspace/rgb30/cores/src
	bump

	tell TG5040
	show ./workspace/tg5040/other/unzip60
	echo CORES
	list ./workspace/tg5040/cores/src
	bump

	tell M17
	echo CORES
	list ./workspace/m17/cores/src
	bump
	
	tell RG35XXPLUS
	show ./workspace/rg35xxplus/other/unzip60
	echo CORES
	list ./workspace/rg35xx/cores/src # just copied from normal rg35xx
	bump
	
	tell GKDPIXEL
	echo CORES
	list ./workspace/gkdpixel/cores/src
	bump
	
	tell MY282
	show ./workspace/my282/other/unzip60
	echo CORES
	list ./workspace/rg35xx/cores/src # just copied from normal rg35xx
	bump
	
	tell MAGICMINI
	show ./workspace/magicmini/other/351files
	echo CORES
	list ./workspace/magicmini/cores/src
	bump

} | sed 's/\n/ /g'