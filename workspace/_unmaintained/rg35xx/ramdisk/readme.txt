can't just hexedit a ramdisk.img because of the internal CRC checksum

make sure we have mkimage

	apt update && apt install u-boot-tools 

(the following steps are slow if performed in the shared workspace folder, 
move ramdisk.img somewhere above that directory to do the work, eg. ~)

strip the header from ramdisk.img

	dd bs=1 skip=64 if=ramdisk.img of=ramdisk-no-header.img

unpack the image

	mkdir ramdisk && cd ramdisk
	cpio -i --no-absolute-filenames < ../ramdisk-no-header.img

patch charger to use /misc/charging.png for _every_ charging image :sweat_smile:

	sed -i 's,/res/images/%s.png,/misc/charging.png,' charger

pack it up and recreate the image

	shopt -s dotglob
	find . | cpio -H newc -o > ../ramdisk.cpio
	mkimage -A arm -O linux -T ramdisk -n "Initial RAM Disk" -d ../ramdisk.cpio ../ramdisk.img.new

based on

	https://boundarydevices.com/hacking-ram-disks/

---

if I decide to use this will need to copy charging.png and patched-ramdisk.img (as ramdisk.img) to /.system/rg35xx/dat