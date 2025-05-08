#!/bin/sh
set -x
PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR")"
PAK_NAME="${PAK_NAME%.*}"

EMU_EXE=prboom
CORES_PATH=$(dirname "$0")

###############################

EMU_TAG=$(basename "$(dirname "$0")" .pak)

rm -f "$LOGS_PATH/$EMU_TAG.txt"
exec >>"$LOGS_PATH/$EMU_TAG.txt"
exec 2>&1

get_doom_version() {
    DOOM_VERSION=""
    if [ -f "$ROM" ]; then
        # get the directory of the file
        ROM_DIR=$(dirname "$ROM")
        if [ -f "$ROM_DIR/doom.version" ]; then
            DOOM_VERSION=$(cat "$ROM_DIR/doom.version")
        fi
    fi
    echo "$DOOM_VERSION"
}

prep_doom_folder() {
    DOOM_VERSION="$1"

    if [ -z "$DOOM_VERSION" ]; then
        return
    fi

    if [ ! -d "$BIOS_PATH/PRBOOM/$DOOM_VERSION" ]; then
        return
    fi

    if [ ! -f "$BIOS_PATH/PRBOOM/$DOOM_VERSION/prboom.wad" ] && [ -f "$BIOS_PATH/PRBOOM/prboom.wad" ]; then
        cp "$BIOS_PATH/PRBOOM/prboom.wad" "$BIOS_PATH/PRBOOM/$DOOM_VERSION/prboom.wad"
    fi

    touch "$BIOS_PATH/PRBOOM/$DOOM_VERSION/prboom.cfg"
    mkdir -p "$BIOS_PATH/PRBOOM/prboom"
    mount -o bind "$BIOS_PATH/PRBOOM/$DOOM_VERSION" "$BIOS_PATH/PRBOOM/prboom"
}

cleanup() {
    umount "$BIOS_PATH/PRBOOM/prboom" || true
}

main() {
    trap "cleanup" EXIT INT TERM HUP QUIT

    ROM="$1"
    mkdir -p "$BIOS_PATH/$EMU_TAG"
    mkdir -p "$SAVES_PATH/$EMU_TAG"
    HOME="$USERDATA_PATH"
    cd "$HOME" || exit 1

    DOOM_VERSION="$(get_doom_version "$ROM")"
    prep_doom_folder "$DOOM_VERSION"

    if [ ! -f "$BIOS_PATH/PRBOOM/prboom/prboom.wad" ] && [ ! -f "$BIOS_PATH/PRBOOM/prboom.wad" ]; then
        echo "No prboom.wad found in $BIOS_PATH/PRBOOM or $BIOS_PATH/PRBOOM/prboom"
        exit 1
    fi

    minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM"
}

main "$@"
