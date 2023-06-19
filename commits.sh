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
	tell UNION
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
	
	tell TRIMUI
	show ./workspace/trimui/other/DinguxCommander
	show ./workspace/trimui/other/sdl
	echo CORES
	list ./workspace/trimui/cores/src
	bump
	
	tell RG353
	show ./workspace/rg353/other/DinguxCommander
	echo CORES
	list ./workspace/rg353/cores/src
	bump

	tell RGNANO
	echo CORES
	list ./workspace/nano/cores/src
	bump

} | sed 's/\n/ /g'