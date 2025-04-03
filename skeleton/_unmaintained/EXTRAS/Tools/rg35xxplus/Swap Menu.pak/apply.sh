#/bin/bash

# LOCALLY with TF1
# backup
# 	dd if=/dev/rdisk4 of=35xxh.part bs=1M count=32
# restore
# 	dd if=35xxh.part of=/dev/rdisk4 bs=1M count=32

killshow()
{
	SHOW_ID=`pidof show.elf`
	kill $SHOW_ID
	wait $SHOW_ID 2>/dev/null
}

DIR="$(dirname "$0")"
cd "$DIR"

show.elf "$DIR/res/preparing.png" 60 &

# PATH doesn't like spaces in paths
cp -rf bin /tmp/
PATH=/tmp/bin:$PATH

# const
DT_NAME=device
DEV_PATH=/dev/mmcblk0
# BOOT_OFFSET=$((16400 * 1024))
# DTB_OFFSET=$((BOOT_OFFSET+1161216))
DTB_OFFSET=17954816
DTB_MAGIC=`xxd -s $DTB_OFFSET -l4 -ps $DEV_PATH`

if [ "$DTB_MAGIC" != "d00dfeed" ]; then
	echo "bad DTB_MAGIC at $DTB_OFFSET"
	DTB_OFFSET=17971200 # alternate dtb location
	DTB_MAGIC=`xxd -s $DTB_OFFSET -l4 -ps $DEV_PATH`
	
	if [ "$DTB_MAGIC" != "d00dfeed" ]; then
		echo "bad DTB_MAGIC at $DTB_OFFSET"
		killshow
		show.elf "$DIR/res/fail.png" 2
		echo "unable to decompile dtb, aborting"
		exit 1
	fi
fi

# var
SIZE_OFFSET=$((DTB_OFFSET+4))
DTB_SIZE=$((0x`xxd -s $SIZE_OFFSET -l4 -ps $DEV_PATH`))

dd if=$DEV_PATH of=$DT_NAME.dtb bs=1 skip=$DTB_OFFSET count=$DTB_SIZE 2>/dev/null # extract
dtc -I dtb -O dts -o $DT_NAME.dts $DT_NAME.dtb 2>/dev/null # decompile

if [ ! -f $DT_NAME.dts ]; then
	killshow
	show.elf "$DIR/res/fail.png" 2
	echo "unable to decompile dtb, aborting"
	exit 1
fi

# dupe so we modify a copy
MOD_PATH=$DT_NAME-mod.dts
cp $DT_NAME.dts $MOD_PATH

# parse the values out of the decompiled dtb
KEY_SELECT=$(sed -n "/keySEl {/,/};/s/.*linux,code = <\(0x[^>]*\)>.*/\1/p" "$MOD_PATH")
KEY_MENU=$(sed -n "/keyMenu {/,/};/s/.*linux,code = <\(0x[^>]*\)>.*/\1/p" "$MOD_PATH")

# swap the values
sed -i "/keySEl {/,/};/s/linux,code = <0x[^>]*>/linux,code = <${KEY_MENU}>/g" "$MOD_PATH"
sed -i "/keyMenu {/,/};/s/linux,code = <0x[^>]*>/linux,code = <${KEY_SELECT}>/g" "$MOD_PATH"

dtc -I dts -O dtb -o $DT_NAME-mod.dtb $DT_NAME-mod.dts 2>/dev/null # recompile
dd if=$DT_NAME.dtb of=$DT_NAME-mod.dtb bs=1 skip=4 seek=4 count=4 conv=notrunc 2>/dev/null # inject original size
fallocate -l $DTB_SIZE $DT_NAME-mod.dtb # zero fill empty space

killshow
show.elf "$DIR/res/applying.png" 60 &

dd if=$DT_NAME-mod.dtb of=$DEV_PATH bs=1 seek=$DTB_OFFSET conv=notrunc 2>/dev/null # inject
sync

killshow
exit 0