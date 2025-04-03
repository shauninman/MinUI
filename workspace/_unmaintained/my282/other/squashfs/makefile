#my282

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

all:
	mkdir -p ./output
	mkdir -p ./output/bin
	mkdir -p ./output/lib
	cp /root/buildroot/output/target/usr/bin/*squashfs ./output/bin/
	cp /root/buildroot/output/target/usr/lib/liblzma.so.5.2.3 ./output/lib/liblzma.so.5

clean:
	rm -rf output