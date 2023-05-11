#!/bin/sh

# move inside the docker image to patch initrd.gz
# otherwise macOS/docker inflates the final filesize
# (probably something to do with the symlinks?)

if [ -d output ]; then
	rm -rf output
fi
mkdir output

TMP_DIR=$(mktemp -d /tmp/boot.XXXX)

cp -r ./ $TMP_DIR

cd $TMP_DIR/output

zcat ../initrd.gz | cpio -idmv
cp ../boot.sh rcS
sed -i 's+exec switch_root+mv /rcS /new_root/etc/init.d\n    exec switch_root+' init
find . | cpio -o --format='newc' | gzip > ~/workspace/rg353/boot/output/initrd.gz

rm -rf $TMP_DIR
