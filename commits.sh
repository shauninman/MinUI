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
	list ./workspace/rg35xx/cores/src
	bump
	tell MIYOOMINI
	list ./workspace/miyoomini/cores/src
} | sed 's/\n/ /g'